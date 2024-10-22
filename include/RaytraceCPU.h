#ifndef __RAYTRACE_CPU_H__
#define __RAYTRACE_CPU_H__

//include parent
#include "Scene.h"

//Graphics API
#include "VulkanHelper.h"

//for imgui and context
#include "AppWideContext.h"

//utilities include
#include "Utilities.h"
#include "Maths.h"

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
	VkPipelineLayout	fullscreenLayout{ VK_NUL_HANDLE };
	//renderpass to full screen the image that is copied from CPU to GPU (one attachement and one output to backbuffers)
	VkRenderPass		fullscreenRenderPass{ VK_NULL_HANDLE };

	//The descriptor to allocate the 
	VkDescriptorPool	CPUImageDescriptorPool{ VK_NULL_HANDLE }

	/*===== END Graphics API =====*/
#pragma endregion

public:
};


#endif //__RAYTRACE_CPU_H__