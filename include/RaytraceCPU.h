#ifndef __RAYTRACE_CPU_H__
#define __RAYTRACE_CPU_H__

//include parent
#include "Scene.h"

//Graphics API
#include "VulkanHelper.h"

//for imgui and context
#include "AppWideContext.h"
#include "Define.h"

//utilities include
#include "RaytraceCPUHelper.h"

//for multithreading
#include <mutex>

//defines for init values
#define RAY_TO_COMPUTE_PER_FRAME 2700

typedef Queue<MultipleSharedMemory<ray_compute>>::QueueNode RayBatch;


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
	* Imports all the data user can change, saved from each use.
	*/
	virtual void Import(rapidjspon::Document& AppSettings)final;

	/*
	* Exports all the data user can change in the document, to save it for next use.
	*/
	virtual void Export(rapidjspon::Document& AppSettings)final;

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
	* the general multithreaded job interface used to compute RayBatches concurrently
	*/
	class RaytraceJob : public ThreadJob
	{
		public:
			RaytraceJob(const RayBatch& RayBatch, const RaytraceCPU& Owner) :
				_Computes{RayBatch},
				_Scene{Owner._Scene},
				_background_gradient_top{ Owner._rtParams._background_gradient_top},
				_background_gradient_bottom{ Owner._rtParams._background_gradient_bottom },
				_depth{ Owner._rtParams._max_depth}
			{
			}

			//the batch this multithreaded job is supposed to accomplish
			RayBatch							_Computes;
			//the scene on which the ray need to be tested
			const ScopedLoopArray<hittable*>&	_Scene;
			//the color for the top of the background gradient 
			vec4								_background_gradient_top;
			//the color for the bottom of the background gradient 
			vec4								_background_gradient_bottom;
			//the max allowed generated rays depth
			uint32_t							_depth;


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
				//the start of our ray heap we need to compute
				const MultipleVolatileMemory<ray_compute> computes = &_Computes.data[_Computes.offset];

				//the nb of rays that hit an object in the scene
				uint32_t hit_nb{ 0 };
				//going over all the rays
				for (uint32_t i = 0; i < _Computes.nb; i++)
				{
					//getting the current ray
					const ray_compute& indexedComputedRay = computes[i];

					if (indexedComputedRay.pixel == nullptr)
						continue;

					//whether this ray has intersected with an object in the scene
					float has_hit{ false };
					//the record of the closest hit between the ray and the objects in the scene
					hit_record closest_hit;
					//basically saying making the distance "INFINITY"
					closest_hit.distance = FLT_MAX;
					for (uint32_t j = 0; j < _Scene.Nb(); j++)
					{
						//compute hit with each object in the scene
						hit_record jHit{};
						if (_Scene[j]->hit(indexedComputedRay.launched, jHit))
						{
							//if we hit the ray intersect with multiple objects, we take the closest to the ray's origin
							if (jHit.distance < closest_hit.distance)
							{
								closest_hit = jHit;
								has_hit = true;
							}
						}
					}

					//giving the pixel color, or generating a bouncing ray
					ProcessRayHit(has_hit, hit_nb, closest_hit, indexedComputedRay);

					//hit_nb helps to record how many rays we may need to generate,but also acts as an index in the heap,
					//that we reuse for generated rays, thus we add it *after* the ray has been processed
					if (has_hit && indexedComputedRay.depth < _depth)
						hit_nb++;
				}

				//if needed, generate a new request to process a new batch of ray compute
				DispatchRayHits(hit_nb);
			}
	};

	static __forceinline vec4 GetRayColor(const ray& incoming, const vec4& background_color_top, const vec4& background_color_bottom)
	{
		float backgroundGradient = 0.5f * (incoming.direction.y + 1.0f);
		return background_color_bottom * (1.0f - backgroundGradient) + background_color_top * backgroundGradient;
	}

	/*
	* the multithreaded job interface used to compute RayBatches from rays after a first contact ray
	*/
	class AnyHitRaytraceJob : public RaytraceJob
	{
	public:
		//the amound each rays contribute to the final image
		float										_sample_weight;
		//the queue in which the job should add its genenrated rays
		Queue<MultipleSharedMemory<ray_compute>>&	_ComputeQueue;
		//fence for safe concurrent had of generated batch in the queue
		std::mutex&									_fence;

		AnyHitRaytraceJob(const RayBatch& RayBatch, RaytraceCPU& Owner) :
			RaytraceJob(RayBatch, Owner),
			_sample_weight{ 1.0f / static_cast<float>(Owner._rtParams._pixel_sample_nb) },
			_ComputeQueue{ Owner._ComputeBatch },
			_fence{ Owner._batch_fence }

		{
		}

		/*
		* Processes the ray_compute depending on the hit status with the scene
		*/
		__forceinline virtual void ProcessRayHit(bool hit, uint32_t ray_index, const hit_record& hit_record, const ray_compute& computed_ray)const final
		{
			//if the ray does not intersect with anything, we may say that it comes from our light source (which in this demo, is "the sky")
			//we make it contribute to the final image, depending on the sample weight (as we launched multiple ray for the same pixel), and with the color of the light
			if (!hit)
				*computed_ray.pixel += computed_ray.color * _sample_weight * GetRayColor(computed_ray.launched, _background_gradient_top, _background_gradient_bottom);
			else if (computed_ray.depth < _depth)//if it hit, we did not found where the light came from, so requesting a new compute to the bounced ray from the hit
			{
				//the new ray to compute
				ray_compute newCompute{};
				
				//the bounced ray
				newCompute.launched = hit_record.mat->propagate(computed_ray.launched, hit_record);
				//still computing the same pixel
				newCompute.pixel = computed_ray.pixel;
				//if it bounced, the object absorbed some of the light's spectrum
				newCompute.color = hit_record.shade * computed_ray.color;
				//adding to the bounce number, as we limit the number of rebounce (as the contribution is close to 0 at a certain point)
				newCompute.depth = computed_ray.depth + 1;
				
				_Computes.data[(_Computes.offset + ray_index)] = newCompute;
			}
		}

		/*
		* Implements the Dispatch of new rays, given the number of hits.
		*/
		__forceinline virtual void DispatchRayHits(uint32_t hit_nb)const final
		{
			//as hit_nb <= _Computes.nb, we recycle the heap to dispatch our rebounced rays.

			//the batch of ray to compute
			RayBatch node;
			node.data	= _Computes.data;//the data was already filled up in the ProcessRay Hits
			node.offset = _Computes.offset;
			node.nb		= hit_nb;

			//sending it away when the resource is available
			_fence.lock();
			_ComputeQueue.PushNode(node);
			_fence.unlock();
		}
	};

	/*
	* the general multithreaded job interface used to compute RayBatches concurrently
	*/
	class FirstContactRaytraceJob : public RaytraceJob
	{
	public:
		//an offset in the ray_compute's heap to not overwrite rays computed now
		uint32_t _offset;
		//the number of rays needed to be generated for a single pixel
		uint32_t _pixel_sample_nb;
		//whether this job should generate rays or not
		bool _is_moving;
		//the queue in which the job should add its genenrated rays
		Queue<MultipleSharedMemory<ray_compute>>&	_ComputeQueue;
		//fence for safe concurrent had of generated batch in the queue
		std::mutex& _fence;


		FirstContactRaytraceJob(const RayBatch& RayBatch, RaytraceCPU& Owner) :
			RaytraceJob(RayBatch, Owner),
			_offset{ Owner._FullScreenScissors.extent.width * Owner._FullScreenScissors.extent.height },
			_pixel_sample_nb{ Owner._rtParams._pixel_sample_nb},
			_is_moving{Owner._is_moving},
			_ComputeQueue{Owner._ComputeBatch},
			_fence{Owner._batch_fence}
		{
		}

		/*
		* Processes the ray_compute depending on the hit status with the scene
		*/
		__forceinline virtual void ProcessRayHit(bool hit, uint32_t ray_index, const hit_record& hit_record, const ray_compute& computed_ray)const final
		{
			if (hit)
			{
				//whether we move changes we should launch new ray or draw a "basic image"
				if (!_is_moving)
				{
					//clearing the screen
					*computed_ray.pixel = vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
					uint32_t globalOffset = _offset + (_Computes.offset + ray_index) * _pixel_sample_nb;
					for (uint32_t i = 0; i < _pixel_sample_nb; i++)
					{
						//the new ray to compute
						ray_compute newCompute{};

						//the bounced ray
						newCompute.launched = hit_record.mat->propagate(computed_ray.launched, hit_record);
						//we want them all to compute the same pixel
						newCompute.pixel = computed_ray.pixel;
						//if it bounced, the object absorbed some of the light's spectrum
						newCompute.color = hit_record.shade;
						//we initialize the number of rebounce, as this is the first hit
						newCompute.depth = 0;

						_Computes.data[globalOffset + i] = newCompute;
					}
				}
				else//helping the user to move into the scene by rendering a simple representation of the scene
					*computed_ray.pixel = hit_record.shade;
			}
			else
			{
				//whether we move or not, we still render the sky
				*computed_ray.pixel = GetRayColor(computed_ray.launched, _background_gradient_top, _background_gradient_bottom);
			}
		}

		/*
		* Implements the Dispatch of new rays, given the number of hits.
		*/
		__forceinline virtual void DispatchRayHits(uint32_t hit_nb)const final
		{
			//dispatching rays when the image is recomputed every frame would be stupid
			if (!_is_moving)
			{
				//the batch of rays to compute
				RayBatch node;
				//the same heap is being used, but also is widly over-allocated to allow this.
				//the size of the heap is basically of one per pixel, then one ray compute for each sample of each pixel
				node.data = _Computes.data;
				//which means the offset is basically : going over the space allocated for the screen
				//then going to the nth pixel considering that every pixel samples
				node.offset	= _offset + _Computes.offset * _pixel_sample_nb;
				//and for every hit, we have at least _pixel_sample_nb ray compute created
				node.nb		= hit_nb * _pixel_sample_nb;

				//sending it away when the resource is available
				_fence.lock();
				_ComputeQueue.PushNode(node);
				_fence.unlock();
			}
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
	ScopedLoopArray<material*>	_Materials;

	//a heap that to allocate the any hit compute heap
	MultipleSharedMemory<ray_compute>			_ComputeHeap;
	//a Queue to append the Ray Batches
	Queue<MultipleSharedMemory<ray_compute>>	_ComputeBatch;
	//a mutex to add and remove the batches concurrently
	std::mutex									_batch_fence;

	//how many rays should we compute per frame refresh ?
	uint32_t	_compute_per_frames{ RAY_TO_COMPUTE_PER_FRAME };
	//parameters useful for raytracing (such as depth or samples)
	RaytracingParams _rtParams;

	//need to recompute the frame
	bool _need_refresh{ true };
	//need to recompute the frame
	bool _is_moving{ false };

	/*===== END CPU Raytracing =====*/
#pragma endregion

};


#endif //__RAYTRACE_CPU_H__
