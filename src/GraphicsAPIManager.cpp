#include "GraphicsAPIManager.h"

//glfw include
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//vulkan include
#include <vulkan/vk_enum_string_helper.h>

#define VK_CALL_PRINT(vk_call)\
	result = vk_call;\
	if (result != VK_SUCCESS)\
	{\
		printf("Vulkan call error : %s ; result : %s .\n", #vk_call, string_VkResult(result));\
	}\
	
//printf include
#include <cstdio>

/*===== Graphics API Manager =====*/

GraphicsAPIManager::~GraphicsAPIManager()
{
	//be careful to free all allocated data
	if (vk_extensions)
		free(vk_extensions);

	vkDestroyInstance(VulkanInterface, nullptr);
}


/*===== Graphics API Support =====*/

bool GraphicsAPIManager::FindAPISupported()
{
	/* Vulkan Support */

	//variable used to gauge if a vulkan call went wrong.
	VkResult result = VK_SUCCESS; 

	//work as already been done so we'll piggy back on glfw method to find out if we Vulkan is supported
	//then for each scene we'll check on scene by scene basis.
	vulkan_supported = glfwVulkanSupported();

	//first, recover the supported extension count of this machine
	VK_CALL_PRINT(vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, nullptr));
	//allocate the space needed to retrieve all the extensions.
	vk_extensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * vk_extension_count);

	printf("\nVulkan extension supported %d. below available extensions:\n", vk_extension_count);

	//retrieve all the extensions
	VK_CALL_PRINT(vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_count, vk_extensions));

	for (uint32_t i = 0; i < vk_extension_count; i++)
	{
		printf("%s.\n", vk_extensions[i]);
	}

	/* DX12 Support */

	/* Metal Support */

	return vulkan_supported ||  DX12_supported || metal_supported;
}

bool GraphicsAPIManager::FindVulkanRTSupported(VkExtensionProperties* PhysicalDeviceExtensionProperties, uint32_t physicalExtensionNb)
{
	//used to count how many of the extensions we need this machine supports
	char needed_extensions = 0;
	//sadly extremely ineffective but necessary look up of all the extensions to see if we support raytracing
	for (uint32_t i = 0; i < physicalExtensionNb; i++)
	{
		if (strcmp(PhysicalDeviceExtensionProperties[i].extensionName, "VK_KHR_acceleration_structure") == 0
			|| strcmp(PhysicalDeviceExtensionProperties[i].extensionName, "VK_KHR_ray_tracing_pipeline") == 0
			|| strcmp(PhysicalDeviceExtensionProperties[i].extensionName, "VK_KHR_ray_query") == 0)
		{
			if (++needed_extensions >= 3)
				break;
		}
	}
	vulkan_rt_supported |= needed_extensions >= 3;

	//finding out if necessary extensions are supported. theoritically
	return needed_extensions >= 3;
}

bool GraphicsAPIManager::FindRTSupported()
{
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

	return vulkan_rt_supported || DXR_supported || metal_rt_supported;
}

/*===== Graphics API Window and Device =====*/

bool GraphicsAPIManager::CreateVulkanInterface()
{
	//to describe the app to the graphics API (directly taken from Vulkan tutorial)
	VkApplicationInfo appInfo{};
	appInfo.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName	= "Vulkan Raytraced-Cel-Shading";
	appInfo.applicationVersion	= VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName			= "No Engine";
	appInfo.engineVersion		= VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion			= VK_API_VERSION_1_3;

	//the struct that will be given to create our VkInstance
	VkInstanceCreateInfo createInfo{};
	createInfo.sType			= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	//the count of extensions needed for glfw to enable interaction with Vulkan
	uint32_t glfwExtensionCount = 0;
	//the names of the extensions needed for glfw to enable interaction with Vulkan
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//create a new variable to add the extensions we want other than those glfw needs
	const char** allExtensions = (const char**)malloc((glfwExtensionCount + 1) * sizeof(const char*));

	//fill up the extensions with the glfw ones
	for (uint32_t extCount = 0; extCount < glfwExtensionCount; extCount++)
		allExtensions[extCount] = glfwExtensions[extCount];

#ifndef NDEBUG
	//add our debug extensions
	allExtensions[glfwExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;


	//get the number of validation layers
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

	//get the validation layers in an array
	VkLayerProperties* layers = (VkLayerProperties*)malloc(layer_count*sizeof(VkLayerProperties));
	vkEnumerateInstanceLayerProperties(&layer_count, layers);

	const char* validationLayer[] = { "VK_LAYER_KHRONOS_validation" };

	//look ing up if the validation layer we want are available
	bool found = false;
	for (uint32_t i = 0; i < layer_count; i++)
	{
		if (strcmp(layers[i].layerName, validationLayer[0]) == 0)
		{
			createInfo.enabledLayerCount = 1;
			createInfo.ppEnabledLayerNames = validationLayer;
			found = true;
			printf("\nVulkan Validation Layer found. Layer Implementation Version : %d ; Layer Spec Version : %d.\n",layers[i].implementationVersion, layers[i].specVersion);
			break;
		}
	}

	if (!found)
	{
		printf("\nVulkan Validation Layer could not be found. moving on without Validation Layers.");
	}

	free(layers);
#endif

	//fill the create struct
	createInfo.enabledExtensionCount	= glfwExtensionCount + 1;
	createInfo.ppEnabledExtensionNames	= allExtensions;


	printf("\nEnabled Extensions:\n");
	for (uint32_t i = 0; i < glfwExtensionCount + 1; i++)
	{
		printf("%s.\n", allExtensions[i]);
	}

	//variable used to gauge if a vulkan call went wrong.
	VkResult result = VK_SUCCESS;

	//creating the instance
	VK_CALL_PRINT(vkCreateInstance(&createInfo, nullptr, &VulkanInterface));
	free(allExtensions);
	return result == VK_SUCCESS;
}

bool GraphicsAPIManager::CreateGraphicsInterfaces()
{
	CreateVulkanInterface();

	return VulkanInterface != nullptr;
}

bool GraphicsAPIManager::CreateVulkanHardwareInterface()
{
	// get the number of physical device
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(VulkanInterface, &deviceCount, nullptr);

	//the list of physical devices currently available for Vulkan
	VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(deviceCount * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(VulkanInterface, &deviceCount, devices);

	//output to console the gpus
	printf("\nVulkan found %d GPU. Below the available devices :\n",deviceCount);

	//The type of the currently chosen GPU
	uint32_t currentDeviceID = 0;
	VkPhysicalDeviceType currentType = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM;
	bool currentRaytracingSupported = false;
	//we'll sort what GPU we want here
	for (uint32_t i = 0; i < deviceCount; i++)
	{
		//the device's properties (name, driver, type etc...)
		VkPhysicalDeviceProperties2 deviceProperty;
 		deviceProperty.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperty.pNext = nullptr;
		//the device's features (geometry shader support, raytracing support, etc...)
		VkPhysicalDeviceFeatures2 deviceFeature;
		deviceFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeature.pNext = nullptr;

		//getting the two structs
		vkGetPhysicalDeviceProperties2(devices[i], &deviceProperty);
		vkGetPhysicalDeviceFeatures2(devices[i], &deviceFeature);

		//output the info to console
		printf("%d - %s.\n",i, deviceProperty.properties.deviceName);

		//for the first GPU, we'll put it as our base line
		if (i == 0)
			currentType = deviceProperty.properties.deviceType;

		//the number of devices extensions 
		uint32_t deviceExtensionNb = 0;
		vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &deviceExtensionNb, nullptr);

		//the device's extension properties
		VkExtensionProperties* GPUExtensions = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * deviceExtensionNb);
		vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &deviceExtensionNb, GPUExtensions);

		//we have two conditions to choose our GPU: 1 - raytracing support.
		bool raytracingSupported = FindVulkanRTSupported(GPUExtensions, deviceExtensionNb);
		if (raytracingSupported && !currentRaytracingSupported)
			currentDeviceID = i;

		// 2 - discrete GPU > everything else.
		if (raytracingSupported 
			&& deviceProperty.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
			&& currentType != VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			currentDeviceID = i;
	}

	//Vulkan Physica Device chosen
	VulkanPhysicalDevice = devices[currentDeviceID];
	
	free(devices);
	return true;
}

bool GraphicsAPIManager::CreateHardwareInterfaces()
{

	CreateVulkanHardwareInterface();

	return VulkanPhysicalDevice != nullptr;
}


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