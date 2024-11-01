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

#define RAY_TO_COMPUTE_PER_FRAME 500


class FirstContactRaytraceJob : public ThreadJob
{
public:
	bool*					ended{ nullptr };
	ray_compute*			computes{ nullptr };
	uint32_t				computesNb{ 0 };
	LoopArray<hittable*>*	hittables{ nullptr };
	Queue<ray_compute>*		newRays{ nullptr };

	__forceinline vec4 GetRayColor(const ray& incoming)
	{
		float backgroundGradient = 0.5f * (incoming.direction.y + 1.0f);
		return vec4{ 1.0f, 1.0f, 1.0f, 1.0f } *(1.0f - backgroundGradient) + vec4{ 0.3f, 0.7f, 1.0f, 1.0f } *backgroundGradient;
	}

	__forceinline void Execute()override
	{
		LoopArray<hittable*>& scene = *hittables;
		for (uint32_t i = 0; i < computesNb; i++)
		{
			const ray_compute& indexedComputedRay = computes[i];

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

			*indexedComputedRay.pixel = hasHit ? final_hit.shade : GetRayColor(indexedComputedRay.launched);

			if (newRays)
			{
				ray_compute newCompute{};
				ray newRay{ final_hit.hit_point, final_hit.hit_normal};
				newRay.direction = newRay.direction + vec3{ randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f) };

				newCompute.launched = newRay;
				newCompute.pixel	= indexedComputedRay.pixel;

				newRays->Push(newCompute);
			}
		}

		if (ended)
			*ended = true;
	}
};

class DiffuseRaytraceJob : public ThreadJob
{
public:
	bool*					ended{ nullptr };
	ray_compute*			computes{ nullptr };
	uint32_t				computesNb{ 0 };
	LoopArray<hittable*>*	hittables{ nullptr };
	Queue<ray_compute>*		newRays{ nullptr };

	__forceinline vec4 GetRayColor(const ray& incoming)
	{
		float backgroundGradient = 0.5f * (incoming.direction.y + 1.0f);
		return vec4{ 1.0f, 1.0f, 1.0f, 1.0f } *(1.0f - backgroundGradient) + vec4{ 0.3f, 0.7f, 1.0f, 1.0f } *backgroundGradient;
	}

	__forceinline void Execute()override
	{
		LoopArray<hittable*>& scene = *hittables;
		for (uint32_t i = 0; i < computesNb; i++)
		{
			const ray_compute& indexedComputedRay = computes[i];

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

			vec4 color = *indexedComputedRay.pixel;
			*indexedComputedRay.pixel = hasHit ? final_hit.shade * color : GetRayColor(indexedComputedRay.launched) * color;

			if (newRays)
			{
				ray_compute newCompute{};
				ray newRay{ final_hit.hit_point, final_hit.hit_normal };
				newRay.direction = newRay.direction + vec3{ randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f) };

				newCompute.launched = newRay;
				newCompute.pixel = indexedComputedRay.pixel;

				newRays->Push(newCompute);
			}
		}

		if (ended)
			*ended = true;
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

	//the pending work executed in act each frames
	Queue<ray_compute>		rayToCompute;

	//how many rays should we compute per frame refresh ?
	uint32_t computePerFrames{ RAY_TO_COMPUTE_PER_FRAME };

	//need to recompute the frame
	bool needRefresh{ true };

};


#endif //__RAYTRACE_CPU_H__