#include "GraphicsAPIManager.h"

//glfw include
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
	
//printf include
#include <cstdio>

/*===== Graphics API Manager =====*/

GraphicsAPIManager::~GraphicsAPIManager()
{
	//be careful to free all allocated data
	if (vk_extensions)
		free(vk_extensions);

	//destroying old frames array (theoretically, the if is useless as free(nullptr) does nothing, but it apparently crashes on some OS)
	if (VulkanBackBuffers)
		free(VulkanBackBuffers);

	//destroy command pools
	if (VulkanCommandPool[0])
		vkDestroyCommandPool(VulkanDevice, VulkanCommandPool[0], nullptr);
	if (VulkanCommandPool[1])
		vkDestroyCommandPool(VulkanDevice, VulkanCommandPool[1], nullptr);

	//destroy semaphores
	if (VulkanCanPresentSemaphore)
	{
		for (uint32_t i = 0; i < NbVulkanFrames; i++)
			vkDestroySemaphore(VulkanDevice, VulkanCanPresentSemaphore[i], nullptr);
		free(VulkanCanPresentSemaphore);
	}
	if (VulkanHasPresentedSemaphore)
	{
		for (uint32_t i = 0; i < NbVulkanFrames; i++)
			vkDestroySemaphore(VulkanDevice, VulkanHasPresentedSemaphore[i], nullptr);
		free(VulkanHasPresentedSemaphore);
	}

	//destroy Fence
	if (VulkanIsDrawingFence)
	{
		for (uint32_t i = 0; i < NbVulkanFrames; i++)
			vkDestroyFence(VulkanDevice, VulkanIsDrawingFence[i], nullptr);
		free(VulkanIsDrawingFence);
	}

	//destroying old framesbuffers associated with frames
	if (VulkanBackColourBuffers)
	{
		for (uint32_t i = 0; i < NbVulkanFrames; i++)
			vkDestroyImageView(VulkanDevice, VulkanBackColourBuffers[i], nullptr);
		free(VulkanBackColourBuffers);
	}

	//vkDestroySwapchainKHR(VulkanDevice, VulkanSwapchain, nullptr);
	vkDestroyDevice(VulkanDevice, nullptr);
	vkDestroySurfaceKHR(VulkanInterface, VulkanSurface, nullptr);
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
	return vulkan_rt_supported || DXR_supported || metal_rt_supported;
}

/*===== Graphics API Window and Device =====*/

bool GraphicsAPIManager::CreateVulkanInterface()
{
	//variable used to gauge if a vulkan call went wrong.
	VkResult result = VK_SUCCESS;

	//to describe the app to the graphics API (directly taken from Vulkan tutorial)
	VkApplicationInfo appInfo{};
	appInfo.sType				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName	= "Vulkan Raytraced-Cel-Shading";
	appInfo.applicationVersion	= VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName			= "No Engine";
	appInfo.engineVersion		= VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion			= VK_HEADER_VERSION_COMPLETE;

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
	VK_CALL_PRINT(vkEnumerateInstanceLayerProperties(&layer_count, nullptr))

	//get the validation layers in an array
	VkLayerProperties* layers = (VkLayerProperties*)malloc(layer_count*sizeof(VkLayerProperties));
	VK_CALL_PRINT(vkEnumerateInstanceLayerProperties(&layer_count, layers))

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
	//variable used to gauge if a vulkan call went wrong.
	VkResult result = VK_SUCCESS;

	// get the number of physical device
	uint32_t deviceCount = 0;
	VK_CALL_PRINT(vkEnumeratePhysicalDevices(VulkanInterface, &deviceCount, nullptr))

	//the list of physical devices currently available for Vulkan
	VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(deviceCount * sizeof(VkPhysicalDevice));
	VK_CALL_PRINT(vkEnumeratePhysicalDevices(VulkanInterface, &deviceCount, devices))

	//output to console the gpus
	printf("\nVulkan found %d GPU. Below the available devices :\n",deviceCount);

	//The type of the currently chosen GPU
	uint32_t currentDeviceID = 0;
	VkPhysicalDeviceType currentType = VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM;
	bool currentRaytracingSupported = false;
	bool canPresent = true;
	//we'll sort what GPU we want here
	for (uint32_t i = 0; i < deviceCount; i++)
	{
		//the device's properties (name, driver, type etc...)
		VkPhysicalDeviceProperties2 deviceProperty{};
 		deviceProperty.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		//the device's features (geometry shader support, raytracing support, etc...)
		VkPhysicalDeviceFeatures2 deviceFeature{};
		deviceFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

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
		VK_CALL_PRINT(vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &deviceExtensionNb, nullptr))

		//the device's extension properties
		VkExtensionProperties* GPUExtensions = (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * deviceExtensionNb);
		VK_CALL_PRINT(vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &deviceExtensionNb, GPUExtensions));

		//we have two conditions to choose our GPU:

		//1 - can it present to screen ?
		bool swapchainsupport = false;
		for (uint32_t i = 0; i < deviceExtensionNb; i++)
		{
			if (strcmp(GPUExtensions[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
			{
				swapchainsupport = true;
				if (swapchainsupport && !canPresent)
				{
					currentDeviceID = i;
					canPresent = true;
				}
				break;
			}
		}

		//2 - raytracing support.
		bool raytracingSupported = FindVulkanRTSupported(GPUExtensions, deviceExtensionNb);
		if (raytracingSupported && !currentRaytracingSupported && swapchainsupport)
		{
			currentDeviceID = i;
			currentRaytracingSupported = true;
		}

		// 3 - discrete GPU > everything else.
		if (swapchainsupport && raytracingSupported 
			&& deviceProperty.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
			&& currentType != VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			currentDeviceID = i;
	}

	VulkanGPU = devices[currentDeviceID];

	//the number of queue family this GPU is capable of 
	//(this entails being able to run compute shader, or if the GPU has video decode/encode cores)
	//Raytracing is considered as a graphic queue, same as rasterizer.
	uint32_t queueFamilyNb = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(VulkanGPU, &queueFamilyNb, nullptr);

	//the queue familty this GPU supports
	VkQueueFamilyProperties* queueFamilyProperties = (VkQueueFamilyProperties*)alloca(sizeof(VkQueueFamilyProperties)*queueFamilyNb);
	memset(queueFamilyProperties, 0, sizeof(VkQueueFamilyProperties) * queueFamilyNb);
	vkGetPhysicalDeviceQueueFamilyProperties(VulkanGPU, &queueFamilyNb, queueFamilyProperties);

	for (; VulkanQueueFamily <= queueFamilyNb; VulkanQueueFamily++)
	{
		if (VulkanQueueFamily == queueFamilyNb)
		{
			queueFamilyNb = 0;
			break;
		}

		if (queueFamilyProperties[VulkanQueueFamily].queueFlags &
			(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
			break;
	}

	//create the queues associated with the device
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = VulkanQueueFamily = queueFamilyNb == 0 ? 0 : VulkanQueueFamily;
	float queuePriorities[2] = { 0.0f, 1.0f };
	queueCreateInfo.pQueuePriorities = queuePriorities;
	queueCreateInfo.queueCount = 2;

	//create the device with the proper extension
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	
	const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_acceleration_structure" , "VK_KHR_ray_tracing_pipeline", "VK_KHR_ray_query", "VK_KHR_deferred_host_operations" };
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	if (vulkan_rt_supported)
	{
		deviceCreateInfo.enabledExtensionCount = 5;
	}
	else
	{
		deviceCreateInfo.enabledExtensionCount = 1;
	}

	//Vulkan Physical Device chosen
	VK_CALL_PRINT(vkCreateDevice(VulkanGPU, &deviceCreateInfo, nullptr, &VulkanDevice))
	
	//announce what GPU we end up with
	{
		VkPhysicalDeviceProperties2 deviceProperty{};
		deviceProperty.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

		vkGetPhysicalDeviceProperties2(VulkanGPU, &deviceProperty);

		printf("Selected %d - %s. Raytracing Supports : %s.\n", currentDeviceID, deviceProperty.properties.deviceName, currentRaytracingSupported ? "true" : "false");
	}

	//Getting back the first queue
	VkDeviceQueueInfo2 QueueInfo{};
	QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	vkGetDeviceQueue2(VulkanDevice, &QueueInfo, &VulkanQueues[0]);
	
	//getting back the second queue
	QueueInfo.queueIndex = 1;
	vkGetDeviceQueue2(VulkanDevice, &QueueInfo, &VulkanQueues[1]);

	{
		//create command pool objects
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = VulkanQueueFamily;

		//making the first command pool
		VK_CALL_PRINT(vkCreateCommandPool(VulkanDevice, &poolInfo, nullptr, &VulkanCommandPool[0]));
		//making the second command pool
		VK_CALL_PRINT(vkCreateCommandPool(VulkanDevice, &poolInfo, nullptr, &VulkanCommandPool[1]));
	}
	

	free(devices);
	return true;
}

bool GraphicsAPIManager::CreateHardwareInterfaces()
{
	CreateVulkanHardwareInterface();

	return VulkanDevice != nullptr;
}


bool GraphicsAPIManager::MakeWindows(GLFWwindow** windows)
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
		VulkanWindow = glfwCreateWindow(1080, 600, window_name, nullptr, nullptr);

		VkResult result = VK_SUCCESS;
		VK_CALL_PRINT(glfwCreateWindowSurface(VulkanInterface, VulkanWindow, nullptr, &VulkanSurface));

		uint32_t formatCount{0};
		vkGetPhysicalDeviceSurfaceFormatsKHR(VulkanGPU, VulkanSurface, &formatCount, nullptr);

		VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)alloca(sizeof(VkSurfaceFormatKHR) * formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(VulkanGPU, VulkanSurface, &formatCount, formats);
		for (uint32_t i = 0; i < formatCount; i++)
		{
			if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB || formats[i].format == VK_FORMAT_B8G8R8A8_SRGB)
			{
				VulkanSurfaceFormat = formats[i];
				break;
			}
		}


		//window creation was successful, let's get surface info so that we'll be able to use it when needing to recreate the framebuffer
		//in case of resize for example.
		if (result == VK_SUCCESS)
		{
			windows[nb_window_init++] = VulkanWindow;
		}
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

	return windows[0] != nullptr || windows[1] != nullptr;
}

bool GraphicsAPIManager::CreateVulkanSwapChain(int32_t width, int32_t height)
{
	//the struct allowing us to know the display capabilities of our hardware
	VkSurfaceCapabilitiesKHR SurfaceCapabilities{};

	//gettting back our display capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanGPU, VulkanSurface, &SurfaceCapabilities);

	//creating the swap chain
	VkSwapchainCreateInfoKHR createinfo{};
	createinfo.sType					= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createinfo.surface					= VulkanSurface;
	createinfo.minImageCount			= SurfaceCapabilities.maxImageCount;
	createinfo.imageFormat				= VulkanSurfaceFormat.format;
	createinfo.imageColorSpace			= VulkanSurfaceFormat.colorSpace;
	createinfo.imageExtent				= VkExtent2D(width, height);
	createinfo.imageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createinfo.queueFamilyIndexCount	= VulkanQueueFamily;
	createinfo.imageArrayLayers			= 1;
	createinfo.compositeAlpha			= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createinfo.imageSharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	createinfo.clipped					= VK_TRUE;
	createinfo.presentMode				= VK_PRESENT_MODE_FIFO_KHR;//for now v_sync
	createinfo.oldSwapchain				= VulkanSwapchain == VK_NULL_HANDLE ? nullptr : VulkanSwapchain;
	createinfo.preTransform				= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	//creating the swapchain
	VkSwapchainKHR tempSwapchain{};
	VK_CALL_PRINT(vkCreateSwapchainKHR(VulkanDevice, &createinfo, nullptr, &tempSwapchain))

	//save if we need to change it afterwards
	VulkanSwapchain = tempSwapchain;

	//destroying old frames array (theoretically, the if is useless as free(nullptr) does nothing, but it apparently crashes on some OS)
	if (VulkanBackBuffers)
		free(VulkanBackBuffers);
	//destroying old framesbuffers associated with frames
	if (VulkanBackColourBuffers)
	{
		for (uint32_t i = 0; i < NbVulkanFrames; i++)
			vkDestroyImageView(VulkanDevice, VulkanBackColourBuffers[i], nullptr);
		free(VulkanBackColourBuffers);
	}

	//get back the number of frames the swapchain was able to create
	VK_CALL_PRINT(vkGetSwapchainImagesKHR(VulkanDevice, VulkanSwapchain, &NbVulkanFrames, nullptr));
	VulkanBackBuffers = (VkImage*)malloc(sizeof(VkImage) * NbVulkanFrames);
	// get back the actual frames
	VK_CALL_PRINT(vkGetSwapchainImagesKHR(VulkanDevice, VulkanSwapchain, &NbVulkanFrames, VulkanBackBuffers));

	//recreate the array of framebuffers associated with the swapchain's frames.
	VulkanBackColourBuffers = (VkImageView*)malloc(sizeof(VkImageView) * NbVulkanFrames);
	
	//describe the ImageView (FrameBuffers) associated with the Frames of the swap chain we want to create
	VkImageViewCreateInfo frameBufferInfo{};
	frameBufferInfo.sType						= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	frameBufferInfo.components					= { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
	frameBufferInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	frameBufferInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;
	frameBufferInfo.format							= VulkanSurfaceFormat.format;
	frameBufferInfo.subresourceRange.baseMipLevel	= 0;
	frameBufferInfo.subresourceRange.levelCount		= 1;
	frameBufferInfo.subresourceRange.baseArrayLayer = 0;
	frameBufferInfo.subresourceRange.layerCount		= 1;

	for (uint32_t i = 0; i < NbVulkanFrames; i++)
	{
		frameBufferInfo.image = VulkanBackBuffers[i];
		VK_CALL_PRINT(vkCreateImageView(VulkanDevice, &frameBufferInfo, nullptr, &VulkanBackColourBuffers[i]));
	}


	//create semaphores and fences
	{

		//destroy semaphores
		if (VulkanCanPresentSemaphore)
		{
			for (uint32_t i = 0; i < NbVulkanFrames; i++)
				vkDestroySemaphore(VulkanDevice, VulkanCanPresentSemaphore[i], nullptr);
			free(VulkanCanPresentSemaphore);
		}
		if (VulkanHasPresentedSemaphore)
		{
			for (uint32_t i = 0; i < NbVulkanFrames; i++)
				vkDestroySemaphore(VulkanDevice, VulkanHasPresentedSemaphore[i], nullptr);
			free(VulkanHasPresentedSemaphore);
		}

		//destroy fence
		if (VulkanIsDrawingFence)
		{
			for (uint32_t i = 0; i < NbVulkanFrames; i++)
				vkDestroyFence(VulkanDevice, VulkanIsDrawingFence[i], nullptr);
			free(VulkanIsDrawingFence);
		}

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		//allocate for new semaphores
		VulkanCanPresentSemaphore	= (VkSemaphore*)malloc(sizeof(VkSemaphore) * NbVulkanFrames);
		VulkanHasPresentedSemaphore = (VkSemaphore*)malloc(sizeof(VkSemaphore) * NbVulkanFrames);
		VulkanIsDrawingFence		= (VkFence*)malloc(sizeof(VkFence) * NbVulkanFrames);

		for (uint32_t i = 0; i < NbVulkanFrames; i++)
		{
			VK_CALL_PRINT(vkCreateSemaphore(VulkanDevice, &semaphoreInfo, nullptr, &VulkanCanPresentSemaphore[i]));
			VK_CALL_PRINT(vkCreateSemaphore(VulkanDevice, &semaphoreInfo, nullptr, &VulkanHasPresentedSemaphore[i]));
			VK_CALL_PRINT(vkCreateFence(VulkanDevice, &fenceInfo, nullptr, &VulkanIsDrawingFence[i]));
		}

	}



	return result == VK_SUCCESS;
}

bool GraphicsAPIManager::MakeSwapChain(GLFWwindow* toAssociateWindow, int32_t width, int32_t height)
{
	/* Vulkan Support */


	if (toAssociateWindow == VulkanWindow)
		return CreateVulkanSwapChain(width, height);

}

/*===== Graphics API Queues and Command =====*/

bool GraphicsAPIManager::GetVulkanNextFrame()
{
	VkResult result = VK_SUCCESS;

	VK_CALL_PRINT(vkAcquireNextImageKHR(VulkanDevice, VulkanSwapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
}

bool GraphicsAPIManager::GetNextFrame()
{

}
