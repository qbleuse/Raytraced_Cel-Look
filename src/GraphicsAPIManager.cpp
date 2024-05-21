#include "GraphicsAPIManager.h"

//glfw include
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//vulkan include
#include <vulkan/vk_enum_string_helper.h>

#define VK_CALL_PRINT(vk_call)\
	{\
		VkResult result = vk_call;\
		if (result != VK_SUCCESS)\
		{\
			printf("Vulkan call error : %s ; result : %s .\n", #vk_call, string_VkResult(result));\
		}\
	}\

//printf include
#include <cstdio>

/*===== Graphics API Manager =====*/

GraphicsAPIManager::~GraphicsAPIManager()
{
	//be careful to free all allocated data
	if (vk_extensions)
		free(vk_extensions);
}


/*===== Graphics API Support =====*/

bool GraphicsAPIManager::FindAPISupported()
{
	/* Vulkan Support */

	//work as already been done so we'll piggy back on glfw method to find out if we Vulkan is supported
	//then for each scene we'll check on scene by scene basis.
	vulkan_supported = glfwVulkanSupported();

	//first, recover the supported extension count of this machine
	VK_CALL_PRINT(vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, nullptr));
	//allocate the space needed to retrieve all the extensions.
	vk_extensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * vk_extension_count);

	printf("vulkan extension supported %d.\n", vk_extension_count);

	//retrieve all the extensions
	VK_CALL_PRINT(vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, vk_extensions));

	//used to count how many of the extensions we need this machine supports
	char needed_extensions = 0;
	//sadly extremely ineffective but necessary look up of all the extensions to see if we support raytracing
	for (uint32_t i = 0; i < vk_extension_count; i++)
	{
		if (strcmp(vk_extensions[i].extensionName, "VK_KHR_acceleration_structure") == 0
			|| strcmp(vk_extensions[i].extensionName, "VK_KHR_ray_tracing_pipeline") == 0
			|| strcmp(vk_extensions[i].extensionName, "VK_KHR_ray_query") == 0)
		{
			if (++needed_extensions >= 3)
				break;
		}
	}

	//finding out if necessary extensions are supported
	vulkan_rt_supported = needed_extensions >= 3;

	/* DX12 Support */

	/* Metal Support */

	return vulkan_supported || vulkan_rt_supported || DX12_supported || DXR_supported || metal_supported || metal_rt_supported;
}

/*===== Graphics API Window and Device =====*/


bool GraphicsAPIManager::MakeWindows(GLFWwindow** windows)const
{
	//the windows we'll return
	*windows = (GLFWwindow*)_malloca(sizeof(GLFWwindow*)*2);
	memset(*windows, 0, sizeof(GLFWwindow*) * 2);
	//the number of initialized window until now
	char nb_window_init = 0;
	//the name of the window to initialize
	const char* window_name = nullptr;

	//disable window manager's Graphics API Integration to create the windows
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	/* Vulkan Support */


	if (vulkan_supported && vulkan_rt_supported)
		window_name = "Vulkan w/ RT\0";
	else if (vulkan_supported)
		window_name = "Vulkan No RT\0";
	
	if (vulkan_supported)
	{
		windows[nb_window_init++] = glfwCreateWindow(800, 600, window_name, nullptr, nullptr);
	}

	/* DirectX Support */

	if (DX12_supported && DXR_supported)
		window_name = "DirectX 12 w/ DXR\0";
	else if (DX12_supported)
		window_name = "DirectX 12 no DXR\0";

	if (DX12_supported)
	{
		windows[nb_window_init++] = glfwCreateWindow(800, 600, window_name, nullptr, nullptr);
	}

	/* Metal Support */


	if (metal_supported && metal_rt_supported)
		window_name = "Metal w/ RT\0";
	else if (metal_supported)
		window_name = "Metal no RT\0";

	if (metal_supported)
	{
		windows[nb_window_init++] = glfwCreateWindow(800, 600, window_name, nullptr, nullptr);
	}

	return windows[0] == nullptr && windows[1] == nullptr;
}