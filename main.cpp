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
#include "ImGuiHelper.h"

//Graphics API include
#include "GraphicsAPIManager.h"

//app include
#include "AppWideContext.h"
#include "Scene.h"
#include "RasterTriangle.h"
#include "RasterObject.h"
#include "RaytraceCPU.h"

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

void FramePresent(GraphicsAPIManager& GAPI, ImGuiResource& imgui, AppWideContext& AppContext)
{
	//if (imgui.SwapChainRebuild)
	//	return;
	VkSemaphore render_complete_semaphore = imgui.VulkanHasDrawnUI[GAPI.RuntimeHandle.VulkanCurrentFrame];
	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.waitSemaphoreCount		= 1;
	info.pWaitSemaphores		= &render_complete_semaphore;
	info.swapchainCount			= 1;
	info.pSwapchains			= &GAPI.VulkanSwapchain;
	info.pImageIndices			= &GAPI.RuntimeHandle.VulkanFrameIndex;
	VkResult err = vkQueuePresentKHR(GAPI.RuntimeHandle.VulkanQueues[0], &info);
	if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
	{
		//imgui.SwapChainRebuild = true;
		return;
	}
	//check_vk_result(err);
	GAPI.RuntimeHandle.VulkanCurrentFrame = (GAPI.RuntimeHandle.VulkanCurrentFrame + 1) % (GAPI.NbVulkanFrames); // Now we can use the next set of semaphores
}

void RefreshAppWideContext(const GraphicsAPIManager& GAPI, AppWideContext& AppContext)
{
	//1. Is UI Opened
	if (!AppContext.ui_visible && ImGui::IsKeyDown(ImGuiKey_U))
	{
		AppContext.ui_visible = true;
	}

	//2. Get Mouse Pos
	{
		ImVec2 MousePos = ImGui::GetMousePos();
		AppContext.prev_mouse_pos = AppContext.mouse_pos;
		AppContext.mouse_pos = { MousePos.x, MousePos.y };
	}

	//3. Get our window's pos
	{
		int x, y;
		glfwGetWindowPos(GAPI.VulkanWindow,&x,&y);

		//for the moment we only have one window so it will be half empty
		AppContext.window_pos = { (float)x, (float)y,0.0f,0.0f };
	}

	// Get our mouse's pos in the window's coordinate
	{
		/*
		*         -1
		*   --------------
		*   |            |
		* -1|            |+1
		*   |            |
		*   --------------
		*         +1
		* 
		* window space is from -1 to +1 in x and y axis
		* so we'll get the mouse's pos from this
		*/

		//first we need to move coordinates from monitor to window space
		vec2 mouse_pos = AppContext.mouse_pos - AppContext.window_pos.xy;

		//then we recenter the corrdinate for {0,0} to be the center of our window
		//same as above, for the moment we only have one window so it will be half empty
		AppContext.window_mouse_pos = { (mouse_pos.x / GAPI.VulkanWidth) * 2.0f - 1.0f, (mouse_pos.y / GAPI.VulkanHeight) * 2.0f - 1.0f , 0.0f, 0.0f};
	}

	AppContext.delta_time = ImGui::GetIO().DeltaTime;

	/* Camera Controls */

	//1. camera mode
	if (ImGui::IsKeyReleased(ImGuiKey_Escape))
	{
		AppContext.in_camera_mode = !AppContext.in_camera_mode;

		glfwSetInputMode(GAPI.VulkanWindow, GLFW_CURSOR, AppContext.in_camera_mode ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	}

	if (AppContext.in_camera_mode)
	{
		//2. camera position
		{
			vec3 right = normalize(vec3{ {AppContext.view_mat[0], AppContext.view_mat[4], AppContext.view_mat[8]} });
			vec3 up = normalize(vec3{ {AppContext.view_mat[1], AppContext.view_mat[5], AppContext.view_mat[9]} });
			vec3 forward = normalize(vec3{ {AppContext.view_mat[2], AppContext.view_mat[6], AppContext.view_mat[10]} });

			if (ImGui::IsKeyDown(ImGuiKey_W))
			{
				AppContext.camera_pos += forward * AppContext.camera_translate_speed * AppContext.delta_time;
			}
			if (ImGui::IsKeyDown(ImGuiKey_S))
			{
				AppContext.camera_pos -= forward * AppContext.camera_translate_speed * AppContext.delta_time;
			}
			if (ImGui::IsKeyDown(ImGuiKey_D))
			{
				AppContext.camera_pos -= right * AppContext.camera_translate_speed * AppContext.delta_time;
			}
			if (ImGui::IsKeyDown(ImGuiKey_A))
			{
				AppContext.camera_pos += right * AppContext.camera_translate_speed * AppContext.delta_time;
			}
			if (ImGui::IsKeyDown(ImGuiKey_Q))
			{
				AppContext.camera_pos -= up * AppContext.camera_translate_speed * AppContext.delta_time;
			}
			if (ImGui::IsKeyDown(ImGuiKey_E))
			{
				AppContext.camera_pos += up * AppContext.camera_translate_speed * AppContext.delta_time;
			}
		}

		//3. camera rotation
		{
			vec2 mouse_move = AppContext.mouse_pos - AppContext.prev_mouse_pos;

			AppContext.camera_rot.y += mouse_move.x * AppContext.camera_rotational_speed * AppContext.delta_time;
			AppContext.camera_rot.x += mouse_move.y * AppContext.camera_rotational_speed * AppContext.delta_time;
		}

		//4. create corresponding view matrix

		AppContext.view_mat = ro_translate(AppContext.camera_pos) * ro_extrinsic_rot(0.0f, AppContext.camera_rot.y, AppContext.camera_rot.x);

		//it is faster to put it by hand
		//AppContext.view_mat[12] = AppContext.camera_pos.x;
		//AppContext.view_mat[13] = AppContext.camera_pos.y;
		//AppContext.view_mat[14] = AppContext.camera_pos.z;
	}

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

		//let the Graphics API create the windows if they're able to. Swap chain for each Graphics API will be created with it.
		if (!GAPI.MakeWindows())
		{
			printf("Graphics API Manager Error: We were not able to the windows associated with the supported Graphics API.\n");
			return 1;
		}

		//init agnostic imgui context
		InitImGui();

		//The imgui Resource stuct regrouping every resource for every graphics interface
		ImGuiResource imguiResource{};
		if (GAPI.vulkan_supported)
			InitImGuiVulkan(GAPI, imguiResource);

		//resources for main loop
		AppWideContext AppContext;
		LoopArray<Scene*> scenes(3);
		scenes[0] = new RasterTriangle();
		scenes[1] = new RasterObject();
		scenes[2] = new RaytraceCPU();

		//init our scenes
		GAPI.PrepareForUpload();
		for (uint32_t i = 0; i < scenes.Nb(); i++)
		{
			scenes[i]->Prepare(GAPI);
			scenes[i]->Resize(GAPI,GAPI.VulkanWidth,GAPI.VulkanHeight,GAPI.NbVulkanFrames);
		}
		GAPI.SubmitUpload();
		AppContext.proj_mat = ro_perspective_proj(GAPI.VulkanWidth, GAPI.VulkanHeight, AppContext.fov, AppContext.near_plane, AppContext.far_plane);

		while (!glfwWindowShouldClose(GAPI.VulkanWindow))
		{
			glfwPollEvents();

			//try getting the next back buffer to draw in the swap chain
			if (!GAPI.GetNextFrame())
			{
				GAPI.ResizeSwapChain(scenes);
				ResetImGuiResource(GAPI, imguiResource);
				AppContext.proj_mat = ro_perspective_proj(GAPI.VulkanWidth, GAPI.VulkanHeight, AppContext.fov, AppContext.near_plane, AppContext.far_plane);
				continue;
			}


			// Start the Dear ImGui frame
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			//1. process input changes and such
			RefreshAppWideContext(GAPI, AppContext);

			//2. draw our ui window
			BeginDrawUIWindow(GAPI, scenes, AppContext);
			
			//3. draw the ui of our scene and react to user input
			scenes[AppContext.scene_index]->Act(AppContext);

			//4. draw said scene
			scenes[AppContext.scene_index]->Show(GAPI.RuntimeHandle);

			//5. draw the ui
			FinishDrawUIWindow(GAPI, imguiResource, AppContext);

			//6. present
			FramePresent(GAPI,imguiResource, AppContext);

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

		for (uint32_t i = 0; i < scenes.Nb(); i++)
		{
			scenes[i]->Close(GAPI);
		}

		ClearImGuiResource(GAPI, imguiResource);
	}

	glfwTerminate();

	return 0;
}
