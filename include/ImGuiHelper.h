#ifndef __IMGUI_HELPER_H__
#define __IMGUI_HELPER_H__

#include "VulkanHelper.h"
#include "Utilities.h"
#include "Define.h"
#include "AppWideContext.h"

struct ImGuiResource
{
	/* Vulkan */

	VkDescriptorPool	_ImGuiDescriptorPool{ VK_NULL_HANDLE };
	VkRenderPass		_ImGuiRenderPass{ VK_NULL_HANDLE };
	
	MultipleScopedMemory<VkCommandBuffer>	_ImGuiCommandBuffer;
	MultipleScopedMemory<VkFramebuffer>		_ImguiFrameBuffer;
	MultipleScopedMemory<VkSemaphore>		_VulkanHasDrawnUI;

};

//forward def
class Scene;


namespace ImGuiHelper
{

	//initialized the resource for Vulkan rendering using the GraphicsAPIManager Resources.
	void InitImGuiVulkan(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource);
	
	//recreate command buffer, framebuffer and semaphore when swapchain changes for every Graphics API used
	bool ResetImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource);
	
	//draws the application wide unified window, but does not end drawing to allow scene to make their UI inside
	void BeginDrawUIWindow(const GraphicsAPIManager& GAPI, ScopedLoopArray<Scene*>& scenes, AppWideContext& AppContext);
	
	//ends drawing end ask to render in the associated Graphics API
	void FinishDrawUIWindow(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource, AppWideContext& AppContext);
	
	//clear all allocated resources to make imgui work with vulkan
	void ClearImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource);
	
	/*===== UI Method =====*/


	//these methods are to make template UI from thing that may be used in multiple places

	//creates the template UI for this transform, and changes the values from user input.
	//sets changed to true if a parameter was edit by user, return true if the panel is being edited
	bool TransformUI(const char* label, Transform& trs, bool& changed);

	//creates the template UI for Raytracing Params, and changes the values from user input
	//sets changed to true if a parameter was edit by user, return true if the panel is being edited
	bool RaytracingParamsUI(const char* label, RaytracingParams& rtParams, bool& changed);

	//creates the template UI to edit a single light, and changes the values from user's input
	//sets changed to true if a parameter was edit by user, return true if the panel is being edited
	bool LightUI(const char* label, Light& light, bool& changed);

	//creates the template UI to edit a Cel-Shading material, and changes the values from user's input
	//sets changed to true if a parameter was edit by user, return true if the panel is being edited
	bool CelParamsUI(const char* label, CelParams& celParams, bool& changed);

};


#endif //__IMGUI_HELPER_H__