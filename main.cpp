/* main.cpp : entry point of the application
* 
*/

//glfw include
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//imgui include

//Graphics API include
#include "GraphicsAPIManager.h"

#include <cstdio>

//directly taken from imgui
static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
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

		if (!GAPI.FindAPISupported())
		{
			printf("Graphics API Manager Error: the application currently supports no Graphics API for the current machine.\n(tried Vulkan %d)\n", GAPI.vulkan_supported);
			return 1;
		}

		if (!GAPI.CreateGraphicsInterfaces())
		{
			printf("Graphics API Manager Error: We were not able to initialize the Logical Interfaces for the supported Graphics API.\n");
			return 1;
		}

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

		if (!GAPI.MakeSwapChain(windows[0], width[0], height[0]))
		{
			printf("Graphics API Manager Error: We were not able to create a swapchain associated with the first window.\n");
			return 1;
		}

		if (windows[1] != nullptr && !GAPI.MakeSwapChain(windows[1], width[1], height[1]))
		{
			printf("Graphics API Manager Error: We were not able to create a swapchain associated with the second window.\n");
			return 1;
		}


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
