/* main.cpp : entry point of the application
* 
*/

//glfw include
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//imgui include
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

//Graphics API include
#include "GraphicsAPIManager.h"

//app include
#include "AppWideContext.h"
#include "Scene.h"
#include "RasterTriangle.h"

#include <cstdio>

//directly taken from imgui
static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

void InitImGui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
}

struct ImGuiResource
{
	/* Vulkan */

	VkDescriptorPool		ImGuiDescriptorPool{VK_NULL_HANDLE};
	VkCommandBuffer*		ImGuiCommandBuffer{ nullptr };
	VkRenderPass			ImGuiRenderPass{ VK_NULL_HANDLE };
	VkFramebuffer*			ImguiFrameBuffer{ nullptr };
	VkSemaphore*			VulkanHasDrawnUI = nullptr;
	bool SwapChainRebuild{ false };

};

bool ResetImGuiWindowResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource, uint32_t width, uint32_t height)
{
	VkResult result = VK_SUCCESS;

	if (ImGuiResource.ImGuiRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(GAPI.VulkanDevice, ImGuiResource.ImGuiRenderPass, nullptr);
	}

	if (ImGuiResource.ImguiFrameBuffer != nullptr)
	{
		for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
			vkDestroyFramebuffer(GAPI.VulkanDevice, ImGuiResource.ImguiFrameBuffer[i], nullptr);
		free(ImGuiResource.ImguiFrameBuffer);
	}

	if (ImGuiResource.ImGuiCommandBuffer != nullptr)
		vkFreeCommandBuffers(GAPI.VulkanDevice, GAPI.VulkanCommandPool[1], GAPI.NbVulkanFrames, ImGuiResource.ImGuiCommandBuffer);

	VK_CLEAR_RAW_ARRAY(ImGuiResource.VulkanHasDrawnUI, GAPI.NbVulkanFrames, vkDestroySemaphore, GAPI.VulkanDevice);


	//basically, no attachement to the UI renderpass
	VkAttachmentDescription attachment = {};
	attachment.format			= GAPI.VulkanSurfaceFormat.format;
	attachment.samples			= VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	//no colour attachment either
	VkAttachmentReference colour_attachment = {};
	colour_attachment.attachment	= 0;
	colour_attachment.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//UI is a rasterized graphic pass
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &colour_attachment;

	//and it usually comes at the end of other graphics pass
	VkSubpassDependency dependency = {};
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask	= 0;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderpass_info = {};
	renderpass_info.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpass_info.attachmentCount = 1;
	renderpass_info.pAttachments	= &attachment;
	renderpass_info.subpassCount	= 1;
	renderpass_info.pSubpasses		= &subpass;
	renderpass_info.dependencyCount	= 1;
	renderpass_info.pDependencies	= &dependency;
	VK_CALL_PRINT(vkCreateRenderPass(GAPI.VulkanDevice, &renderpass_info, nullptr, &ImGuiResource.ImGuiRenderPass));

	VkImageView backbuffers[1];
	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType				= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass			= ImGuiResource.ImGuiRenderPass;
	framebuffer_info.attachmentCount	= 1;
	framebuffer_info.pAttachments		= backbuffers;
	framebuffer_info.width				= width;
	framebuffer_info.height				= height;
	framebuffer_info.layers				= 1;

	ImGuiResource.ImguiFrameBuffer = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * GAPI.NbVulkanFrames);
	ImGuiResource.VulkanHasDrawnUI = (VkSemaphore*)malloc(sizeof(VkSemaphore) * GAPI.NbVulkanFrames);

	for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
	{
		backbuffers[0] = GAPI.VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI.VulkanDevice, &framebuffer_info, nullptr, &ImGuiResource.ImguiFrameBuffer[i]));

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VK_CALL_PRINT(vkCreateSemaphore(GAPI.VulkanDevice, &semaphoreInfo, nullptr, &ImGuiResource.VulkanHasDrawnUI[i]));
	}

	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = GAPI.VulkanCommandPool[1];
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = GAPI.NbVulkanFrames;

		ImGuiResource.ImGuiCommandBuffer = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * GAPI.NbVulkanFrames);
		VK_CALL_PRINT(vkAllocateCommandBuffers(GAPI.VulkanDevice, &allocInfo, ImGuiResource.ImGuiCommandBuffer));
	}
	
	return result == VK_SUCCESS;
}

void ClearImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource)
{
	vkDestroyDescriptorPool(GAPI.VulkanDevice, ImGuiResource.ImGuiDescriptorPool, nullptr);

	if (ImGuiResource.ImGuiCommandBuffer != nullptr)
		vkFreeCommandBuffers(GAPI.VulkanDevice, GAPI.VulkanCommandPool[1], GAPI.NbVulkanFrames, ImGuiResource.ImGuiCommandBuffer);

	if (ImGuiResource.ImGuiRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(GAPI.VulkanDevice, ImGuiResource.ImGuiRenderPass, nullptr);
	}

	if (ImGuiResource.ImguiFrameBuffer != nullptr)
	{
		for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
			vkDestroyFramebuffer(GAPI.VulkanDevice, ImGuiResource.ImguiFrameBuffer[i], nullptr);
		free(ImGuiResource.ImguiFrameBuffer);
	}

	VK_CLEAR_RAW_ARRAY(ImGuiResource.VulkanHasDrawnUI, GAPI.NbVulkanFrames, vkDestroySemaphore, GAPI.VulkanDevice);

}

void InitImGuiVulkan(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource, int32_t width, int32_t height)
{
	//create a GPU Buffer for ImGUI textures
	{
		VkResult result = VK_SUCCESS;

		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1;
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		
		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI.VulkanDevice, &pool_info, nullptr, &ImGuiResource.ImGuiDescriptorPool));
	}

	ResetImGuiWindowResource(GAPI, ImGuiResource, width, height);

	ImGui_ImplGlfw_InitForVulkan(GAPI.VulkanWindow, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance			= GAPI.VulkanInterface;
	init_info.PhysicalDevice	= GAPI.VulkanGPU;
	init_info.Device			= GAPI.VulkanDevice;
	init_info.QueueFamily		= GAPI.VulkanQueueFamily;
	init_info.Queue				= GAPI.VulkanQueues[1];
	init_info.PipelineCache		= VK_NULL_HANDLE;
	init_info.DescriptorPool	= ImGuiResource.ImGuiDescriptorPool;
	init_info.RenderPass		= ImGuiResource.ImGuiRenderPass;
	init_info.Subpass			= 0;
	init_info.MinImageCount		= 3;
	init_info.ImageCount		= GAPI.NbVulkanFrames;
	init_info.MSAASamples		= VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator			= nullptr;
	init_info.CheckVkResultFn	= nullptr;
	ImGui_ImplVulkan_Init(&init_info);

}

void FrameRender(const GraphicsAPIManager& GAPI, ImGuiResource& imgui, ImDrawData* draw_data, int32_t width, int32_t height)
{
	if (imgui.SwapChainRebuild)
		return;
	VkResult err;

	{
		err = vkResetCommandBuffer(imgui.ImGuiCommandBuffer[GAPI.VulkanFrameIndex], 0);
		//check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(imgui.ImGuiCommandBuffer[GAPI.VulkanFrameIndex], &info);
	//	check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass					= imgui.ImGuiRenderPass;
		info.framebuffer				= imgui.ImguiFrameBuffer[GAPI.VulkanFrameIndex];
		info.renderArea.extent.width	= width;
		info.renderArea.extent.height	= height;
		info.clearValueCount			= 0;
		//info.pClearValues				= &imgui.ImGuiWindow.ClearValue;
		vkCmdBeginRenderPass(imgui.ImGuiCommandBuffer[GAPI.VulkanFrameIndex], &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Record dear imgui primitives into command buffer
	ImGui_ImplVulkan_RenderDrawData(draw_data, imgui.ImGuiCommandBuffer[GAPI.VulkanFrameIndex]);

	// Submit command buffer
	vkCmdEndRenderPass(imgui.ImGuiCommandBuffer[GAPI.VulkanFrameIndex]);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkSubmitInfo info = {};
		info.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores	= &GAPI.VulkanHasPresentedSemaphore[GAPI.VulkanCurrentFrame];
		info.pWaitDstStageMask	= &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers	= &imgui.ImGuiCommandBuffer[GAPI.VulkanFrameIndex];
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores	= &imgui.VulkanHasDrawnUI[GAPI.VulkanCurrentFrame];

		err = vkEndCommandBuffer(imgui.ImGuiCommandBuffer[GAPI.VulkanFrameIndex]);
		//check_vk_result(err);
		err = vkQueueSubmit(GAPI.VulkanQueues[1], 1, &info, nullptr);
		//check_vk_result(err);
	}
}

void FramePresent( GraphicsAPIManager& GAPI, ImGuiResource& imgui)
{
	if (imgui.SwapChainRebuild)
		return;
	VkSemaphore render_complete_semaphore = imgui.VulkanHasDrawnUI[GAPI.VulkanCurrentFrame];
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount		= 1;
	info.pWaitSemaphores		= &render_complete_semaphore;
	info.swapchainCount			= 1;
	info.pSwapchains			= &GAPI.VulkanSwapchain;
	info.pImageIndices			= &GAPI.VulkanFrameIndex;
	VkResult err = vkQueuePresentKHR(GAPI.VulkanQueues[1], &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		imgui.SwapChainRebuild = true;
		return;
	}
	//check_vk_result(err);
	GAPI.VulkanCurrentFrame = (GAPI.VulkanCurrentFrame + 1) % (GAPI.NbVulkanFrames); // Now we can use the next set of semaphores
}

int main()
{
	//set error callback and init window manager lib
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return 1;

	//this is so that destructor are called before the end of the program
	{

		//object that will handle Graphics API initialization and global resource management
		GraphicsAPIManager GAPI;

		//finds supported API
		if (!GAPI.FindAPISupported())
		{
			printf("Graphics API Manager Error: the application currently supports no Graphics API for the current machine.\n(tried Vulkan %d)\n", GAPI.vulkan_supported);
			return 1;
		}

		//Create logical devices (Graphics API Interfaces object to handle resources)
		if (!GAPI.CreateGraphicsInterfaces())
		{
			printf("Graphics API Manager Error: We were not able to initialize the Logical Interfaces for the supported Graphics API.\n");
			return 1;
		}

		//Create Physical devices (Graphics API Interfaces object that represent the GPU)
		if (!GAPI.CreateHardwareInterfaces())
		{
			printf("Graphics API Manager Error: We were not able to initialize the Hardware Interfaces for the supported Graphics API.\n");
			return 1;
		}

		//let the Graphics API create the windows if they're able to
 		GLFWwindow* windows[2] = {nullptr, nullptr};
		if (!GAPI.MakeWindows(windows))
		{
			printf("Graphics API Manager Error: We were not able to the windows associated with the supported Graphics API.\n");
			return 1;
		}

		//the width and height of the windows
		int32_t width[2], height[2] = { 0, 0};
		glfwGetFramebufferSize(windows[0], &width[0], &height[0]);
		if (windows[1] != nullptr)
			glfwGetFramebufferSize(windows[1], &width[1], &height[1]);

		//create the swap chains
		for (int32_t i = 0; i < 2; i++)
			if (windows[i] != nullptr)
				if (!GAPI.MakeSwapChain(windows[i], width[i], height[i]))
				{
					printf("Graphics API Manager Error: We were not able to create a swapchain associated with the first window.\n");
					return 1;
				}

		//init agnostic imgui context
		InitImGui();

		//The imgui Resource stuct regrouping every resource for every graphics interface
		ImGuiResource imguiResource{};
		if (GAPI.vulkan_supported)
			InitImGuiVulkan(GAPI, imguiResource, width[0], height[0]);

		//resources for main loop
		bool show_demo_window = true;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		AppWideContext AppContext;
		AppContext.ImContext = ImGui::GetIO();

		Scene* scene[1] = { new RasterTriangle() };

		scene[0]->Prepare(GAPI);
		scene[0]->Resize(GAPI, width[0], height[0]);

		while (!glfwWindowShouldClose(windows[0]))
		{
			if (windows[1] != nullptr && !glfwWindowShouldClose(windows[1]))
				break;

			glfwPollEvents();

			//try getting the next back buffer to draw in the swap chain
			if (!GAPI.GetNextFrame())
			{
				imguiResource.SwapChainRebuild = true;
			}

			//resize window
			int fb_width, fb_height;
			for (char i = 0; i < 2; i++)
			{
				if (windows[i] != nullptr)
				{
					glfwGetFramebufferSize(windows[i], &fb_width, &fb_height);
					if (fb_width > 0 && fb_height > 0 && (imguiResource.SwapChainRebuild || width[i] != fb_width || height[i] != fb_height))
					{
						GAPI.MakeSwapChain(windows[i], fb_width, fb_height);
						ResetImGuiWindowResource(GAPI, imguiResource, fb_width, fb_height);
						scene[0]->Resize(GAPI, width[0], height[0]);
						imguiResource.SwapChainRebuild = false;
						continue;
					}
				}
			}


			// Start the Dear ImGui frame
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			scene[0]->Show(GAPI);

			//do scene stuff here.

			// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			// UI render spot
			ImGui::Render();
			ImDrawData* draw_data = ImGui::GetDrawData();
			const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
			//if (!is_minimized)
			{

				FrameRender(GAPI,imguiResource, draw_data, width[0], height[0]);
				FramePresent(GAPI,imguiResource);
			}


			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault(NULL, imguiResource.ImGuiRenderPass);
			}

		}

		vkDeviceWaitIdle(GAPI.VulkanDevice);
		//check_vk_result(err);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		scene[0]->Close(GAPI);

		delete scene[0];

		ClearImGuiResource(GAPI, imguiResource);

		glfwDestroyWindow(windows[0]);
		glfwDestroyWindow(windows[1]);
	}

	glfwTerminate();

	return 0;
}
