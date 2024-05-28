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

		//let the Graphics API create the windows if they're able to
 		GLFWwindow* windows[2] = {nullptr, nullptr};
		GAPI.MakeWindows(windows);

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
