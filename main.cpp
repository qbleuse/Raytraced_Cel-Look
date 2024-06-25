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

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
}

void InitImGuiVulkan(const GraphicsAPIManager& GAPI, ImGui_ImplVulkanH_Window& ImGuiWindow, int32_t width, int32_t height)
{
	
	//set FrameBuffer Format and ColorSpace
	const VkFormat wantedFormat = VK_FORMAT_R8G8B8_UNORM;
	const VkColorSpaceKHR wantedColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	ImGuiWindow.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(GAPI.VulkanGPU, GAPI.VulkanSurface, &wantedFormat, 1, wantedColorSpace);

	//set present mode (for now, we'll choose simple vsync)
	VkPresentModeKHR vsync =  VK_PRESENT_MODE_FIFO_KHR ;
	ImGuiWindow.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(GAPI.VulkanGPU, GAPI.VulkanSurface, &vsync, 1);

	//set Surface
	ImGuiWindow.Surface = GAPI.VulkanSurface;

	//create ImGui FrameBuffer
	ImGui_ImplVulkanH_CreateOrResizeWindow(GAPI.VulkanInterface, GAPI.VulkanGPU, GAPI.VulkanDevice, &ImGuiWindow, 1, nullptr, width, height, 3);

	ImGui_ImplGlfw_InitForVulkan(GAPI.VulkanWindow, true);
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance			= GAPI.VulkanInterface;
	init_info.PhysicalDevice	= GAPI.VulkanGPU;
	init_info.Device			= GAPI.VulkanDevice;
	init_info.QueueFamily		= 0;
	init_info.Queue				= GAPI.VulkanQueues[1];
	init_info.PipelineCache		= VK_NULL_HANDLE;
	//init_info.DescriptorPool	= g_DescriptorPool;
	//init_info.RenderPass		= wd->RenderPass;
	init_info.Subpass			= 0;
	init_info.MinImageCount		= 3;
	init_info.ImageCount		= 3;
	init_info.MSAASamples		= VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator			= nullptr;
	init_info.CheckVkResultFn	= nullptr;
	ImGui_ImplVulkan_Init(&init_info);

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

		//The imgui Vulkan Implementation Interface
		//ImGui_ImplVulkanH_Window imguiVulkanInterface{};
		//if (GAPI.vulkan_supported)
		//	InitImGuiVulkan(GAPI, imguiVulkanInterface, width[0], height[0]);


		while (!glfwWindowShouldClose(windows[0]))
		{
			if (windows[1] != nullptr && !glfwWindowShouldClose(windows[1]))
				break;

			glfwPollEvents();
		}

		glfwDestroyWindow(windows[0]);
		glfwDestroyWindow(windows[1]);
	}

	glfwTerminate();

	return 0;
}
