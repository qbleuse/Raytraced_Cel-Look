#ifndef __IMGUI_HELPER_H__
#define __IMGUI_HELPER_H__

#include "VulkanHelper.h"
#include "Utilities.h"
#include "AppWideContext.h"

struct ImGuiResource
{
	/* Vulkan */

	VkDescriptorPool	ImGuiDescriptorPool{ VK_NULL_HANDLE };
	VkRenderPass		ImGuiRenderPass{ VK_NULL_HANDLE };
	
	HeapMemory<VkCommandBuffer>	ImGuiCommandBuffer;
	HeapMemory<VkFramebuffer>		ImguiFrameBuffer;
	HeapMemory<VkSemaphore>		VulkanHasDrawnUI;

};

//initialized the resource for Vulkan rendering using the GraphicsAPIManager Resources.
void InitImGuiVulkan(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource);

//recreate command buffer, framebuffer and semaphore when swapchain changes for every Graphics API used
bool ResetImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource);

//draws the application wide unified window, but does not end drawing to allow scene to make their UI inside
void BeginDrawUIWindow(const GraphicsAPIManager& GAPI, SmartLoopArray<class Scene*>& scenes, AppWideContext& AppContext);

//ends drawing end ask to render in the associated Graphics API
void FinishDrawUIWindow(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource, AppWideContext& AppContext);

//clear all allocated resources to make imgui work with vulkan
void ClearImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource);


#endif //__IMGUI_HELPER_H__