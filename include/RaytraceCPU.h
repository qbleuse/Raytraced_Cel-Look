#ifndef __RAYTRACE_CPU_H__
#define __RAYTRACE_CPU_H__

//include parent
#include "Scene.h"

//Graphics API
#include "VulkanHelper.h"

//for imgui and context
#include "AppWideContext.h"

//utilities include
#include "RaytracingHelper.h"

//for multithreading
#include <mutex>

#define RAY_TO_COMPUTE_PER_FRAME 500

typedef SmartQueue<SharedSmartHeapMemory<ray_compute>>::QueueNode RayBatch;

class FirstContactRaytraceJob : public ThreadJob
{
public:
	bool*							ended{ nullptr };
	RayBatch						computes{ nullptr };
	LoopArray<hittable*>*			hittables{ nullptr };
	SmartQueue<SharedSmartHeapMemory<ray_compute>>*		newRays{ nullptr };
	std::mutex*						newRaysFence{ nullptr };
	std::condition_variable_any*	newRays_wait{nullptr};

	__forceinline vec4 GetRayColor(const ray& incoming)
	{
		float backgroundGradient = 0.5f * (incoming.direction.y + 1.0f);
		return vec4{ 1.0f, 1.0f, 1.0f, 1.0f } *(1.0f - backgroundGradient) + vec4{ 0.3f, 0.7f, 1.0f, 1.0f } *backgroundGradient;
	}

	__forceinline vec4 GetColorFromHit(LoopArray<hittable*>& scene, ray_compute& computes, vec4& finalColor, uint32_t depth)
	{
		if (depth == 0)
			return vec4{ 0.0f, 0.0f, 0.0f, 0.0f };

		float hasHit = false;
		hit_record final_hit{};
		final_hit.distance = FLT_MAX;
		for (uint32_t i = 0; i < scene.Nb(); i++)
		{
			hit_record iHit{};
			if (scene[i]->hit(computes.launched, iHit))
			{
				if (iHit.distance < final_hit.distance)
				{
					final_hit = iHit;
					hasHit = true;
				}
			}
		}

		if (hasHit)
		{
			ray_compute newCompute{};
			ray newRay{ final_hit.hit_point, final_hit.hit_normal };
			newRay.direction = newRay.direction + normalize(vec3{ randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f) });

			newCompute.launched = newRay;
			//newCompute.pixel = indexedComputedRay.pixel;


			return GetColorFromHit(scene, newCompute, finalColor, depth-1) * 0.5f;
		}

		return GetRayColor(computes.launched);
	}

	__forceinline void Execute()override
	{
		LoopArray<hittable*>& scene = *hittables;
		SharedSmartHeapMemory<ray_compute> gen_rays{ computes.nb * 10};
		uint32_t hit_nb{0};
		for (uint32_t i = 0; i < computes.nb; i++)
		{
			ray_compute& indexedComputedRay = computes.data[i + computes.index];
			
			//*indexedComputedRay.pixel += GetColorFromHit(scene, indexedComputedRay, *indexedComputedRay.pixel, 10);

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
			
			if (hasHit && newRays != nullptr)
			{
				for (uint32_t i = 0; i < 10; i++)
				{
					ray_compute newCompute{};

					newCompute.launched = final_hit.mat->reflect(indexedComputedRay.launched,final_hit);
					newCompute.pixel = indexedComputedRay.pixel;
					newCompute.color = final_hit.shade;

					gen_rays[hit_nb++] = newCompute;
				}
			}
			
			//basically clearing screen
			if (!hasHit)
				*indexedComputedRay.pixel = GetRayColor(indexedComputedRay.launched);
			else
				*indexedComputedRay.pixel = vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
		}

		if (newRays && newRaysFence && newRays_wait && hit_nb > 0)
		{
			newRaysFence->lock();
		
			newRays->PushBatch(gen_rays, hit_nb);
			newRaysFence->unlock();
		}

		if (ended)
			*ended = true;

		//free(computes);
	}
};

class DiffuseRaytraceJob : public ThreadJob
{
public:
	bool*							ended{ nullptr };
	RayBatch						computes{ nullptr };
	LoopArray<hittable*>*			hittables{ nullptr };
	SmartQueue<SharedSmartHeapMemory<ray_compute>>*		newRays{ nullptr };
	std::mutex*						newRaysFence{ nullptr };
	std::condition_variable_any*	newRays_wait{ nullptr };


	__forceinline vec4 GetRayColor(const ray& incoming)
	{
		float backgroundGradient = 0.5f * (incoming.direction.y + 1.0f);
		return vec4{ 1.0f, 1.0f, 1.0f, 1.0f } *(1.0f - backgroundGradient) + vec4{ 0.3f, 0.7f, 1.0f, 1.0f } *backgroundGradient;
	}

	__forceinline void Execute()override
	{
		LoopArray<hittable*>& scene = *hittables;
		SharedSmartHeapMemory<ray_compute> gen_rays{ computes.nb};
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
				*indexedComputedRay.pixel += indexedComputedRay.color * 0.1f;

			if (hasHit && newRays != nullptr)
			{
				ray_compute newCompute{};

				newCompute.launched = final_hit.mat->reflect(indexedComputedRay.launched,final_hit);
				newCompute.pixel = indexedComputedRay.pixel;
				newCompute.color = final_hit.shade * indexedComputedRay.color;

				gen_rays[hit_nb++] = newCompute;
			}
		}

		if (newRays && newRaysFence && newRays_wait && hit_nb > 0)
		{
			newRaysFence->lock();

			newRays->PushBatch(gen_rays, hit_nb);
			newRaysFence->unlock();
		}

		if (ended)
			*ended = true;

		//free(computes);
	}
};


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
	VkPipeline			fullscreenPipeline{VK_NULL_HANDLE};
	//layout to render to full screen the image that is copied from CPU to GPU (a single sampler)
	VkPipelineLayout	fullscreenLayout{ VK_NULL_HANDLE };
	//renderpass to full screen the image that is copied from CPU to GPU (one attachement and one output to backbuffers)
	VkRenderPass		fullscreenRenderPass{ VK_NULL_HANDLE };

	//The descriptor to allocate the descriptor of the local GPU image
	VkDescriptorPool			GPUImageDescriptorPool{ VK_NULL_HANDLE };
	//the descriptor layout for the sampler of the local GPU image
	VkDescriptorSetLayout		GPUImageDescriptorLayout{ VK_NULL_HANDLE };
	//the sampler used to sample the GPU local images
	VkSampler					fullscreenSampler{ VK_NULL_HANDLE };
	//the actual descriptors for the sampler of the GPU image, with one for each frame
	HeapMemory<VkDescriptorSet>	GPUImageDescriptorSets;
	//the framebuffer to present to after fullscreen copy
	HeapMemory<VkFramebuffer>	fullscreenOutput;


	//a viewport that covers the entire screen
	VkViewport	fullscreenViewport{};
	//a scrissors that covers the entire screen
	VkRect2D	fullscreenScissors{};

	//the Buffer object associated with the local image on the GPU
	HeapMemory<VkImage>		GPULocalImageBuffers;
	//the view object associated with the local image on the GPU
	HeapMemory<VkImageView>	GPULocalImageViews;
	//the allocated memory on GPU in which the local image is
	VkDeviceMemory			GPULocalImageMemory{VK_NULL_HANDLE};
	//the size of a single ImageBuffer on GPU
	VkDeviceSize			GPULocalImageBufferSize{0};


	//the Buffer object that stages the new image each frame
	HeapMemory<VkBuffer>	ImageCopyBuffer;
	//the allocated memory on GPU mapped on CPU memory
	VkDeviceMemory			ImageCopyMemory;
	//the mapped CPU memory that the GPU copies to get the image
	HeapMemory<void*>		MappedCPUImage;
	//the size of a single copy Buffer on GPU
	VkDeviceSize			ImageCopyBufferSize{ 0 };

	void PrepareVulkanProps(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	void ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames);

	/*===== END Graphics API =====*/
#pragma endregion

#pragma region SCENE INTERFACE 
public:
	/*===== Scene Interface =====*/

	virtual void Prepare(class GraphicsAPIManager& GAPI)final;

	virtual void Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)final;

	__forceinline virtual const char* Name()override { return "CPU Raytracing (Path)"; }

	virtual void Act(struct AppWideContext& AppContext)final;

	void DispatchSceneRay(struct AppWideContext& AppContext);

	virtual void Show(GAPIHandle& GAPIHandle)final;

	virtual void Close(class GraphicsAPIManager& GAPI)final;

	// the cpu image. should be of size width * height
	HeapMemory<vec4>		raytracedImage;

	//the objects in our scene
	LoopArray<hittable*>	scene;
	//the materials for our objects
	SmartLoopArray<material*> materials;

	//the pending work executed in act each frames
	SmartQueue<SharedSmartHeapMemory<ray_compute>>	rayToCompute;
	//the mutex for this data (as it will be changed by multiple threads at a time)
	std::mutex					queue_fence;
	//the condition variable to make the thread wait or wake up.
	std::condition_variable_any queue_wait;

	//how many rays should we compute per frame refresh ?
	uint32_t computePerFrames{ RAY_TO_COMPUTE_PER_FRAME };

	//need to recompute the frame
	bool needRefresh{ true };

};


#endif //__RAYTRACE_CPU_H__