#include "GraphicsAPIManager.h"

//glfw include
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//project include
#include "Scene.h"

//printf include
#include <cstdio>

/*===== Graphics API Manager =====*/

GraphicsAPIManager::~GraphicsAPIManager()
{
	//be careful to free all allocated data
	if (vk_extensions)
		free(vk_extensions);

	//destroying old frames array (theoretically, the if is useless as free(nullptr) does nothing, but it apparently crashes on some OS)
	VulkanBackBuffers.Clear();
	//destroying old framesbuffers associated with frames
	VK_CLEAR_ARRAY(VulkanBackColourBuffers, NbVulkanFrames, vkDestroyImageView, VulkanDevice);
	//destroy semaphores
	VK_CLEAR_ARRAY(RuntimeHandle.VulkanCanPresentSemaphore, NbVulkanFrames, vkDestroySemaphore, VulkanDevice);
	VK_CLEAR_ARRAY(RuntimeHandle.VulkanHasPresentedSemaphore, NbVulkanFrames, vkDestroySemaphore, VulkanDevice);
	//destroy fence
	VK_CLEAR_ARRAY(RuntimeHandle.VulkanIsDrawingFence, NbVulkanFrames, vkDestroyFence, VulkanDevice);
	//destroy command buffers (we want the same number of command buffer as backbuffers)
	if (RuntimeHandle.VulkanCommand != nullptr)
	{
		vkFreeCommandBuffers(VulkanDevice, VulkanCommandPool[0], NbVulkanFrames, *RuntimeHandle.VulkanCommand);
		RuntimeHandle.VulkanCommand.Clear();
	}

	//destroy command pools
	if (VulkanCommandPool[0])
		vkDestroyCommandPool(VulkanDevice, VulkanCommandPool[0], nullptr);
	if (VulkanCommandPool[1])
		vkDestroyCommandPool(VulkanDevice, VulkanCommandPool[1], nullptr);


	vkDestroySwapchainKHR(VulkanDevice, VulkanSwapchain, nullptr);
	vkDestroyDevice(VulkanDevice, nullptr);
	vkDestroySurfaceKHR(VulkanInterface, VulkanSurface, nullptr);
	vkDestroyInstance(VulkanInterface, nullptr);

	glfwDestroyWindow(VulkanWindow);
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
		printf("%s.\n", vk_extensions[i].extensionName);
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
	vkGetDeviceQueue2(VulkanDevice, &QueueInfo, &RuntimeHandle.VulkanQueues[0]);

	//getting back the second queue
	QueueInfo.queueIndex = 1;
	vkGetDeviceQueue2(VulkanDevice, &QueueInfo, &RuntimeHandle.VulkanQueues[1]);

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


bool GraphicsAPIManager::MakeWindows()
{
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

		glfwGetFramebufferSize(VulkanWindow, &VulkanWidth, &VulkanHeight);
		ResizeVulkanSwapChain(VulkanWidth, VulkanHeight);

		//window creation was successful, let's get surface info so that we'll be able to use it when needing to recreate the framebuffer
		//in case of resize for example.
		if (result == VK_SUCCESS)
		{
			nb_window_init++;
		}
	}

	/* DirectX Support */

	if (DX12_supported && DXR_supported)
		window_name = "DirectX 12 w/ DXR\0";
	else if (DX12_supported)
		window_name = "DirectX 12 no DXR\0";

	if (DX12_supported)
	{
		//windows[nb_window_init++] = glfwCreateWindow(800, 600, window_name, nullptr, nullptr);
	}

	/* Metal Support */


	if (metal_supported && metal_rt_supported)
		window_name = "Metal w/ RT\0";
	else if (metal_supported)
		window_name = "Metal no RT\0";

	if (metal_supported)
	{
		//windows[nb_window_init++] = glfwCreateWindow(800, 600, window_name, nullptr, nullptr);
	}

	return nb_window_init > 0;
}

bool GraphicsAPIManager::ResizeVulkanSwapChain(int32_t width, int32_t height)
{
	vkDeviceWaitIdle(VulkanDevice);

	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	//destroying old frames array (theoretically, the if is useless as free(nullptr) does nothing, but it apparently crashes on some OS)
	VulkanBackBuffers.Clear();
	//destroying old framesbuffers associated with frames
	VK_CLEAR_ARRAY(VulkanBackColourBuffers, NbVulkanFrames, vkDestroyImageView, VulkanDevice);
	//destroy semaphores
	VK_CLEAR_ARRAY(RuntimeHandle.VulkanCanPresentSemaphore, NbVulkanFrames, vkDestroySemaphore, VulkanDevice);
	VK_CLEAR_ARRAY(RuntimeHandle.VulkanHasPresentedSemaphore, NbVulkanFrames, vkDestroySemaphore, VulkanDevice);
	//destroy fence
	VK_CLEAR_ARRAY(RuntimeHandle.VulkanIsDrawingFence, NbVulkanFrames, vkDestroyFence, VulkanDevice);
	//destroy command buffers (we want the same number of command buffer as backbuffers)
	if (RuntimeHandle.VulkanCommand != nullptr)
	{
		vkFreeCommandBuffers(VulkanDevice, VulkanCommandPool[0], NbVulkanFrames, *RuntimeHandle.VulkanCommand);
		RuntimeHandle.VulkanCommand.Clear();
	}

	//the struct allowing us to know the display capabilities of our hardware
	VkSurfaceCapabilitiesKHR SurfaceCapabilities{};

	//gettting back our display capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanGPU, VulkanSurface, &SurfaceCapabilities);

	VkSwapchainKHR tempSwapchain{ VulkanSwapchain };

	//creating the swap chain
	VkSwapchainCreateInfoKHR createinfo{};
	createinfo.sType					= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createinfo.surface					= VulkanSurface;
	createinfo.minImageCount			= SurfaceCapabilities.maxImageCount;
	createinfo.imageFormat				= VulkanSurfaceFormat.format;
	createinfo.imageColorSpace			= VulkanSurfaceFormat.colorSpace;
	createinfo.imageExtent				= VkExtent2D{(uint32_t)width, (uint32_t)height};
	createinfo.imageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createinfo.queueFamilyIndexCount	= VulkanQueueFamily;
	createinfo.imageArrayLayers			= 1;
	createinfo.compositeAlpha			= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createinfo.imageSharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	createinfo.clipped					= VK_TRUE;
	createinfo.presentMode				= VK_PRESENT_MODE_FIFO_KHR;//for now v_sync
	createinfo.oldSwapchain				= tempSwapchain;
	createinfo.preTransform				= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	//creating the swapchain
	VK_CALL_PRINT(vkCreateSwapchainKHR(VulkanDevice, &createinfo, nullptr, &VulkanSwapchain))

	vkDestroySwapchainKHR(VulkanDevice, tempSwapchain, nullptr);

	//get back the number of frames the swapchain was able to create
	VK_CALL_PRINT(vkGetSwapchainImagesKHR(VulkanDevice, VulkanSwapchain, &NbVulkanFrames, nullptr));
	RuntimeHandle.NbVulkanFrames = NbVulkanFrames;
	VulkanWidth = width;
	VulkanHeight = height;
	VulkanBackBuffers.Alloc(NbVulkanFrames);
	// get back the actual frames
	VK_CALL_PRINT(vkGetSwapchainImagesKHR(VulkanDevice, VulkanSwapchain, &NbVulkanFrames, *VulkanBackBuffers));

	//recreate the array of framebuffers associated with the swapchain's frames.
	VulkanBackColourBuffers.Alloc(NbVulkanFrames);

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
		VK_CALL_PRINT(vkCreateImageView(VulkanDevice, &frameBufferInfo, nullptr, &(VulkanBackColourBuffers[i])));
	}

	//create semaphores and fences
	{

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		//allocate for new semaphores

		RuntimeHandle.VulkanCanPresentSemaphore.Alloc(NbVulkanFrames);
		RuntimeHandle.VulkanHasPresentedSemaphore.Alloc(NbVulkanFrames);
		RuntimeHandle.VulkanIsDrawingFence.Alloc(NbVulkanFrames);

		for (uint32_t i = 0; i < NbVulkanFrames; i++)
		{
			VK_CALL_PRINT(vkCreateSemaphore(VulkanDevice, &semaphoreInfo, nullptr, &RuntimeHandle.VulkanCanPresentSemaphore[i]));
			VK_CALL_PRINT(vkCreateSemaphore(VulkanDevice, &semaphoreInfo, nullptr, &RuntimeHandle.VulkanHasPresentedSemaphore[i]));
			VK_CALL_PRINT(vkCreateFence(VulkanDevice, &fenceInfo, nullptr, &RuntimeHandle.VulkanIsDrawingFence[i]));
		}

	}

	//create commmand buffers
	{

		//first allocate
		RuntimeHandle.VulkanCommand.Alloc(NbVulkanFrames);

		//then fill up the command buffers in the array
		VkCommandBufferAllocateInfo command_info{};
		command_info.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		command_info.commandPool		= VulkanCommandPool[0];
		command_info.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		command_info.commandBufferCount = NbVulkanFrames;

		vkAllocateCommandBuffers(VulkanDevice, &command_info, *RuntimeHandle.VulkanCommand);
	}

	return result == VK_SUCCESS;
}

bool GraphicsAPIManager::ResizeSwapChain(NumberedArray<Scene*>& SceneToChange)
{
	/* Vulkan Support */

	if (vulkan_supported)
	{
		int32_t old_width = VulkanWidth;
		int32_t old_height = VulkanHeight;
		uint32_t oldNbFrames = NbVulkanFrames;

		glfwGetFramebufferSize(VulkanWindow, &VulkanWidth, &VulkanHeight);

		bool toReturn = ResizeVulkanSwapChain(VulkanWidth, VulkanHeight);

		if (toReturn)
			for (uint32_t i = 0; i < SceneToChange.Nb(); i++)
				SceneToChange[i]->Resize(*this, old_width, old_height, oldNbFrames);

		return toReturn;

	}

	return false;
}

/*===== Graphics API Queues and Command =====*/

bool GraphicsAPIManager::GetVulkanNextFrame()
{
	VkResult result = VK_SUCCESS;

	{
		//if we already looped through all the frames in our swapchain, but this one did not finish drawing, we have no other choice than wait
		vkWaitForFences(VulkanDevice, 1, &RuntimeHandle.VulkanIsDrawingFence[RuntimeHandle.VulkanCurrentFrame], VK_TRUE, UINT64_MAX);

		//if we did not wait or
		vkResetFences(VulkanDevice, 1, &RuntimeHandle.VulkanIsDrawingFence[RuntimeHandle.VulkanCurrentFrame]);
	}

	VK_CALL_PRINT(vkAcquireNextImageKHR(VulkanDevice, VulkanSwapchain, UINT64_MAX, RuntimeHandle.VulkanCanPresentSemaphore[RuntimeHandle.VulkanCurrentFrame], VK_NULL_HANDLE, &RuntimeHandle.VulkanFrameIndex));

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		return false;

	//VK_CALL_PRINT(vkResetCommandPool(VulkanDevice, VulkanCommandPool[0], 0));
	//VK_CALL_PRINT(vkResetCommandPool(VulkanDevice, VulkanCommandPool[1], 0));

	int32_t width, height {0};

	glfwGetFramebufferSize(VulkanWindow, &width, &height);

	return VulkanWidth == width && VulkanHeight == height;
}

bool GraphicsAPIManager::GetNextFrame()
{
	return GetVulkanNextFrame();
}
