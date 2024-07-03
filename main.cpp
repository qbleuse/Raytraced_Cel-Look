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

	ImGui_ImplVulkanH_Window ImGuiWindow;
	VkDescriptorPool ImGuiDescriptorPool;
	bool SwapChainRebuild{ false };

};

void InitImGuiVulkan(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource, int32_t width, int32_t height)
{

	//create a GPU Buffer for ImGUI textures
	{
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
		vkCreateDescriptorPool(GAPI.VulkanDevice, &pool_info, nullptr, &ImGuiResource.ImGuiDescriptorPool);
	}

	//set FrameBuffer Format and ColorSpace
	const VkFormat wantedFormat = GAPI.VulkanSurfaceFormat.format;
	const VkColorSpaceKHR wantedColorSpace = GAPI.VulkanSurfaceFormat.colorSpace;
	ImGuiResource.ImGuiWindow.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(GAPI.VulkanGPU, GAPI.VulkanSurface, &wantedFormat, 1, wantedColorSpace);

	//set present mode (for now, we'll choose simple vsync)
	VkPresentModeKHR vsync =  VK_PRESENT_MODE_FIFO_KHR ;
	ImGuiResource.ImGuiWindow.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(GAPI.VulkanGPU, GAPI.VulkanSurface, &vsync, 1);

	//set Surface
	ImGuiResource.ImGuiWindow.Surface = GAPI.VulkanSurface;

	//create ImGui FrameBuffer
	ImGui_ImplVulkanH_CreateOrResizeWindow(GAPI.VulkanInterface, GAPI.VulkanGPU, GAPI.VulkanDevice, &ImGuiResource.ImGuiWindow, GAPI.VulkanQueueFamily, nullptr, width, height, 3);

	ImGui_ImplGlfw_InitForVulkan(GAPI.VulkanWindow, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance			= GAPI.VulkanInterface;
	init_info.PhysicalDevice	= GAPI.VulkanGPU;
	init_info.Device			= GAPI.VulkanDevice;
	init_info.QueueFamily		= 0;
	init_info.Queue				= GAPI.VulkanQueues[1];
	init_info.PipelineCache		= VK_NULL_HANDLE;
	init_info.DescriptorPool	= ImGuiResource.ImGuiDescriptorPool;
	init_info.RenderPass		= ImGuiResource.ImGuiWindow.RenderPass;
	init_info.Subpass			= 0;
	init_info.MinImageCount		= 3;
	init_info.ImageCount		= 3;
	init_info.MSAASamples		= VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator			= nullptr;
	init_info.CheckVkResultFn	= nullptr;
	ImGui_ImplVulkan_Init(&init_info);

}

void FrameRender(const GraphicsAPIManager& GAPI, ImGuiResource& imgui, ImDrawData* draw_data)
{
	if (imgui.SwapChainRebuild)
		return;
	VkResult err;

	VkSemaphore image_acquired_semaphore = imgui.ImGuiWindow.FrameSemaphores[imgui.ImGuiWindow.SemaphoreIndex].ImageAcquiredSemaphore;
	VkSemaphore render_complete_semaphore = imgui.ImGuiWindow.FrameSemaphores[imgui.ImGuiWindow.SemaphoreIndex].RenderCompleteSemaphore;
	err = vkAcquireNextImageKHR(GAPI.VulkanDevice, GAPI.VulkanSwapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &imgui.ImGuiWindow.FrameIndex);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		imgui.SwapChainRebuild = true;
		return;
	}

	ImGui_ImplVulkanH_Frame* fd = &imgui.ImGuiWindow.Frames[imgui.ImGuiWindow.FrameIndex];
	{
		err = vkWaitForFences(GAPI.VulkanDevice, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
		//check_vk_result(err);

		err = vkResetFences(GAPI.VulkanDevice, 1, &fd->Fence);
		//check_vk_result(err);
	}
	{
		err = vkResetCommandPool(GAPI.VulkanDevice, fd->CommandPool, 0);
		//check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
	//	check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = imgui.ImGuiWindow.RenderPass;
		info.framebuffer = fd->Framebuffer;
		info.renderArea.extent.width = imgui.ImGuiWindow.Width;
		info.renderArea.extent.height = imgui.ImGuiWindow.Height;
		info.clearValueCount = 1;
		info.pClearValues = &imgui.ImGuiWindow.ClearValue;
		vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	// Record dear imgui primitives into command buffer
	ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

	// Submit command buffer
	vkCmdEndRenderPass(fd->CommandBuffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &image_acquired_semaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &fd->CommandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &render_complete_semaphore;

		err = vkEndCommandBuffer(fd->CommandBuffer);
		//check_vk_result(err);
		err = vkQueueSubmit(GAPI.VulkanQueues[1], 1, &info, fd->Fence);
		//check_vk_result(err);
	}
}

void FramePresent(const GraphicsAPIManager& GAPI, ImGuiResource& imgui)
{
	if (imgui.SwapChainRebuild)
		return;
	VkSemaphore render_complete_semaphore = imgui.ImGuiWindow.FrameSemaphores[imgui.ImGuiWindow.SemaphoreIndex].RenderCompleteSemaphore;
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &render_complete_semaphore;
	info.swapchainCount = 1;
	info.pSwapchains = &imgui.ImGuiWindow.Swapchain;
	info.pImageIndices = &imgui.ImGuiWindow.FrameIndex;
	VkResult err = vkQueuePresentKHR(GAPI.VulkanQueues[1], &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		imgui.SwapChainRebuild = true;
		return;
	}
	//check_vk_result(err);
	imgui.ImGuiWindow.SemaphoreIndex = (imgui.ImGuiWindow.SemaphoreIndex + 1) % imgui.ImGuiWindow.SemaphoreCount; // Now we can use the next set of semaphores
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

		while (!glfwWindowShouldClose(windows[0]))
		{
			if (windows[1] != nullptr && !glfwWindowShouldClose(windows[1]))
				break;

			glfwPollEvents();

			//resize window
			int fb_width, fb_height;
			for (char i = 0; i < 2; i++)
			{
				if (windows[i] != nullptr)
				{
					glfwGetFramebufferSize(windows[i], &fb_width, &fb_height);
					if (fb_width > 0 && fb_height > 0 && (imguiResource.SwapChainRebuild || width[i] != fb_width || height[i] != fb_height))
					{
						//GAPI.MakeSwapChain(windows[i], fb_width, fb_height);

						ImGui_ImplVulkan_SetMinImageCount(3);
						ImGui_ImplVulkanH_CreateOrResizeWindow(GAPI.VulkanInterface, GAPI.VulkanGPU, GAPI.VulkanDevice, &imguiResource.ImGuiWindow, GAPI.VulkanQueueFamily, nullptr, fb_width, fb_height, 3);
						imguiResource.ImGuiWindow.FrameIndex = 0;
						imguiResource.SwapChainRebuild = false;
					}
				}
			}

			// Start the Dear ImGui frame
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
			if (show_demo_window)
				ImGui::ShowDemoWindow(&show_demo_window);

			// Rendering
			ImGui::Render();
			ImDrawData* draw_data = ImGui::GetDrawData();
			const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
			if (!is_minimized)
			{
				imguiResource.ImGuiWindow.ClearValue.color.float32[0] = clear_color.x * clear_color.w;
				imguiResource.ImGuiWindow.ClearValue.color.float32[1] = clear_color.y * clear_color.w;
				imguiResource.ImGuiWindow.ClearValue.color.float32[2] = clear_color.z * clear_color.w;
				imguiResource.ImGuiWindow.ClearValue.color.float32[3] = clear_color.w;
				FrameRender(GAPI,imguiResource, draw_data);
				FramePresent(GAPI,imguiResource);
			}


			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault(NULL, imguiResource.ImGuiWindow.RenderPass);
			}

		}

		vkDeviceWaitIdle(GAPI.VulkanDevice);
		//check_vk_result(err);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		ImGui_ImplVulkanH_DestroyWindow(GAPI.VulkanInterface, GAPI.VulkanDevice, &imguiResource.ImGuiWindow, nullptr);
		vkDestroyDescriptorPool(GAPI.VulkanDevice, imguiResource.ImGuiDescriptorPool, nullptr);

		glfwDestroyWindow(windows[0]);
		glfwDestroyWindow(windows[1]);
	}

	glfwTerminate();

	return 0;
}
