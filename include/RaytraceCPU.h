#ifndef __RAYTRACE_CPU_H__
#define __RAYTRACE_CPU_H__

//include parent
#include "Scene.h"

//Graphics API
#include "VulkanHelper.h"

//for imgui and context
#include "AppWideContext.h"

//utilities include
#include "RaytraceCPUHelper.h"

//for multithreading
#include <mutex>

#define RAY_TO_COMPUTE_PER_FRAME 2700
#define PIXEL_SAMPLE_NB 50

/*class AnyHitRaytraceJob : public ThreadJob
{
public:
	RayBatch						computes{ nullptr };
	ScopedLoopArray<hittable*>*		hittables{ nullptr };
	float							sampleWeight{0.0f};

	__forceinline vec4 GetRayColor(const ray& incoming)
	{
		float backgroundGradient = 0.5f * (incoming.direction.y + 1.0f);
		return vec4{ 1.0f, 1.0f, 1.0f, 1.0f } *(1.0f - backgroundGradient) + vec4{ 0.3f, 0.7f, 1.0f, 1.0f } *backgroundGradient;
	}

	__forceinline void Execute()override
	{
		ScopedLoopArray<hittable*>& scene = *hittables;
		//SharedHeapMemory<ray_compute> gen_rays{ computes.nb};
		uint32_t hit_nb{ 0 };
		for (uint32_t i = 0; i < computes.nb; i++)
		{
			const ray_compute& indexedComputedRay = computes.data[i + computes.index];

			float hasHit = false;
			hit_record final_hit{};
			final_hit.distance = FLT_MAX;
			for (uint32_t i = 0; i < scene.Nb(); i++)
			{
				hit_record iHit{};
				if (scene[i]->hit(indexedComputedRay.launched, iHit))
				{
					if (iHit.distance < final_hit.distance)
					{
						final_hit = iHit;
						hasHit = true;
					}
				}
			}

			if (!hasHit && indexedComputedRay.pixel != nullptr)
				*indexedComputedRay.pixel += indexedComputedRay.color * sampleWeight * GetRayColor(indexedComputedRay.launched);

			if (hasHit && newRays != nullptr)
			{
				ray_compute newCompute{};

				newCompute.launched = final_hit.mat->reflect(indexedComputedRay.launched,final_hit);
				newCompute.pixel = indexedComputedRay.pixel;
				newCompute.color = final_hit.shade * indexedComputedRay.color;

				computes.data[hit_nb++] = newCompute;
			}
		}

		if (newRays && newRaysFence && newRays_wait && hit_nb > 0)
		{
			newRaysFence->lock();
			computes.nb = hit_nb;
			newRays->PushNode(computes);
			newRaysFence->unlock();
		}

		if (ended)
			*ended = true;

		//free(computes);
	}
};*/


/**
* This class is a scene to do the Raytracing in One Week-End tutorial.
* Doing the actual rendering on CPU then copying the image onto the GPU afterwards
*/
class RaytraceCPU : public Scene
{
#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//pipeline to render to full screen the image that is copied from CPU to GPU
	VkPipeline			_FullScreenPipeline{VK_NULL_HANDLE};
	//layout to render to full screen the image that is copied from CPU to GPU (a single sampler)
	VkPipelineLayout	_FullScreenLayout{ VK_NULL_HANDLE };
	//renderpass to full screen the image that is copied from CPU to GPU (one attachement and one output to backbuffers)
	VkRenderPass		_FullScreenRenderPass{ VK_NULL_HANDLE };

	//The descriptor to allocate the descriptor of the local GPU image
	VkDescriptorPool						_GPUImageDescriptorPool{ VK_NULL_HANDLE };
	//the descriptor layout for the sampler of the local GPU image
	VkDescriptorSetLayout					_GPUImageDescriptorLayout{ VK_NULL_HANDLE };
	//the sampler used to sample the GPU local images
	VkSampler								_FullScreenSampler{ VK_NULL_HANDLE };
	//the actual descriptors for the sampler of the GPU image, with one for each frame
	MultipleScopedMemory<VkDescriptorSet>	_GPUImageDescriptorSets;
	//the framebuffer to present to after fullscreen copy
	MultipleScopedMemory<VkFramebuffer>		_FullScreenOutput;


	//a viewport that covers the entire screen
	VkViewport	_FullScreenViewport{};
	//a scrissors that covers the entire screen
	VkRect2D	_FullScreenScissors{};

	//the Buffer object associated with the local image on the GPU
	MultipleScopedMemory<VkImage>		_GPULocalImageBuffers;
	//the view object associated with the local image on the GPU
	MultipleScopedMemory<VkImageView>	_GPULocalImageViews;
	//the allocated memory on GPU in which the local image is
	VkDeviceMemory						_GPULocalImageMemory{VK_NULL_HANDLE};
	//the size of a single ImageBuffer on GPU
	VkDeviceSize						_GPULocalImageBufferSize{0};


	//the Buffer object that stages the new image each frame
	MultipleScopedMemory<VkBuffer>	_ImageCopyBuffer;
	//the allocated memory on GPU mapped on CPU memory
	VkDeviceMemory					_ImageCopyMemory;
	//the mapped CPU memory that the GPU copies to get the image
	MultipleScopedMemory<void*>		_MappedCPUImage;
	//the size of a single copy Buffer on GPU
	VkDeviceSize					_ImageCopyBufferSize{ 0 };

	/*
	* Creates the necessary resources for displaying with vulkan.
	* This includes:
	* - RenderPass Object
	* - DescriptorSet Layouts
	* - Pipeline Layout
	* - Pipeline Object
	* - Sampler Object
	*/
	void PrepareVulkanProps(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	
	/*
	* Compiles the shaders to use in the Pipeline Object creation
	*/
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	
	/*
	* Deallocate previously allocated resources, then recreate resources using the window's new properties.
	* This includes:
	* - Viewport (though not allocated)
	* - Scissors (though not allocated)
	* - Framebuffers
	* - CPU image Buffers
	* - GPU images
	* - Descriptor sets objects
	*/
	void ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames);

	/*===== END Graphics API =====*/
#pragma endregion

#pragma region SCENE INTERFACE 
public:
	/*===== Scene Interface =====*/

	/*
	* Prepares once all the unmovable resources needed (such as pipelines in recent Graphics APIs)
	*/
	virtual void Prepare(class GraphicsAPIManager& GAPI)final;

	/*
	* Allocates the resources associated with window (which can change size during runtime), and deallocate previously allocated resources if needed.
	* This is expected to be called multiple time, do not put resources that are supposed to be created once in this method.
	*/
	virtual void Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)final;

	/*
	* The name of this scene. This can be hardcoded or a member, but it should never fail.
	*/
	__forceinline virtual const char* Name()const noexcept override { return "CPU Raytracing (Path)"; }

	/*
	* called once per frame.
	* Changes data inside the scene depending on time or user input that can be found in the AppContext (or with ImGUI)
	* UI should also be updated here.
	*/
	virtual void Act(struct AppWideContext& AppContext)final;

	/*
	* called once per frame.
	* Uses the data changed in Act and the resources to display on screen the scene's current context.
	* Should create and send the render command to the GPU.
	*/
	virtual void Show(GAPIHandle& GAPIHandle)final;

	/*
	* Releases all the resources allocated in Prepare
	*/
	virtual void Close(class GraphicsAPIManager& GAPI)final;



	/*===== END Scene Interface =====*/
#pragma endregion


#pragma region CPU RAYTRACING
public:
	/*===== CPU Raytracing =====*/

	/*
	* a simple struct to represent a group of ray_compute to process together
	*/
	struct RayBatch
	{
		//the shared memory used to create new computes
		MultipleSharedMemory<ray_compute> _Heap{ nullptr };

		//the index in this heap memory where this batch starts
		uint32_t index{ 0 };
		//the number of computes in this batch
		uint32_t nb{ 0 };
	};

	/*
	* the general multithreaded job interface used to compute RayBatches concurrently
	*/
	class RaytraceJob : public ThreadJob
	{
		public:
			RaytraceJob(const RayBatch& RayBatch, ThreadPool* ThreadPool, const RaytraceCPU& Owner) :
				_Computes{RayBatch},
				_ThreadPool{ThreadPool},
				_Owner{Owner}
			{
			}

			//the batch this multithreaded job is supposed to accomplish
			RayBatch						_Computes{ nullptr };
			//the pool on which this job should be executed
			ThreadPool*						_ThreadPool{nullptr};
			//the owner of this job
			const RaytraceCPU&				_Owner;

			/*
			* Processes the ray_compute depending on the hit status with the scene
			*/
			virtual void ProcessRayHit(bool hit, uint32_t ray_index, const hit_record& hit_record, const ray_compute& computed_ray)const = 0;

			/*
			* Implements the Dispatch of new rays, given the number of hits.
			*/
			virtual void DispatchRayHits(uint32_t hit_nb)const = 0;

			__forceinline void Execute()override
			{
				if (_Owner._need_refresh)
					return;

				const ScopedLoopArray<hittable*>& scene = _Owner._Scene;

				uint32_t hit_nb{ 0 };
				for (uint32_t i = 0; i < _Computes.nb; i++)
				{
					const ray_compute& indexedComputedRay = _Computes._Heap[i + _Computes.index];

					float hasHit = false;
					hit_record final_hit{};
					final_hit.distance = FLT_MAX;
					for (uint32_t j = 0; j < scene.Nb(); j++)
					{
						hit_record jHit{};
						if (scene[j]->hit(indexedComputedRay.launched, jHit))
						{
							if (jHit.distance < final_hit.distance)
							{
								final_hit = jHit;
								hasHit = true;
							}
						}
					}

					if (hasHit)
						hit_nb++;

					ProcessRayHit(hasHit, hit_nb, final_hit, indexedComputedRay);
				}

				if (_Owner._need_refresh)
					return;

				DispatchRayHits(hit_nb);
			}
	};

	__forceinline vec4 GetRayColor(const ray& incoming)const
	{
		float backgroundGradient = 0.5f * (incoming.direction.y + 1.0f);
		return vec4{ 1.0f, 1.0f, 1.0f, 1.0f } *(1.0f - backgroundGradient) + vec4{ 0.3f, 0.7f, 1.0f, 1.0f } *backgroundGradient;
	}

	/*
	* the general multithreaded job interface used to compute RayBatches concurrently
	*/
	class FirstContactRaytraceJob : public RaytraceJob
	{
	public:


		FirstContactRaytraceJob(const RayBatch& RayBatch, ThreadPool* ThreadPool, const RaytraceCPU& Owner) :
			RaytraceJob(RayBatch, ThreadPool, Owner)
		{
		}

		/*
		* Processes the ray_compute depending on the hit status with the scene
		*/
		__forceinline virtual void ProcessRayHit(bool hit, uint32_t ray_index, const hit_record& hit_record, const ray_compute& computed_ray)const final
		{
			if (hit)
			{
				if (_ThreadPool)
				{
					*computed_ray.pixel = vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
					for (uint32_t i = 0; i < _Owner._pixel_sample_nb; i++)
					{
						ray_compute newCompute{};

						newCompute.launched = hit_record.mat->propagate(computed_ray.launched, hit_record);
						newCompute.pixel = computed_ray.pixel;
						newCompute.color = hit_record.shade;

						_Computes._Heap[_Computes.index * _Owner._pixel_sample_nb + ray_index] = newCompute;
					}
				}
				else
					*computed_ray.pixel = hit_record.shade;
			}
			else
			{
				*computed_ray.pixel = _Owner.GetRayColor(computed_ray.launched);
			}
		}

		/*
		* Implements the Dispatch of new rays, given the number of hits.
		*/
		__forceinline virtual void DispatchRayHits(uint32_t hit_nb)const final
		{
			//TODO : NOT IMPLEMENTED

			//if (_ThreadPool)
			//{
			//
			//}
		}
	};


	/*
	* called every time the scene changes or need a redraw.
	* deallocates every CPU resources used draw the current version of teh raytraced scene;
	* and reallocate new resources to draw the current version of the scene.
	* This includes the creation of multithreaded jobs.
	*/
	void DispatchSceneRay(struct AppWideContext& AppContext);

	// the cpu image. should be of size width * height
	MultipleScopedMemory<vec4>	_RaytracedImage;

	//the objects in our scene
	ScopedLoopArray<hittable*>	_Scene;
	//the materials for our objects
	ScopedLoopArray<material*> _Materials;

	//a heap that to allocate the any hit compute heap
	MultipleSharedMemory<ray_compute>			_ComputeHeap;

	//how many rays should we compute per frame refresh ?
	uint32_t _compute_per_frames{ RAY_TO_COMPUTE_PER_FRAME };
	//the number of sample for a single pixel
	uint32_t _pixel_sample_nb{ PIXEL_SAMPLE_NB };

	//need to recompute the frame
	bool _need_refresh{ true };

	/*===== END CPU Raytracing =====*/
#pragma endregion

};


#endif //__RAYTRACE_CPU_H__