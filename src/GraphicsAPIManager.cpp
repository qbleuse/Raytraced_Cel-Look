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
	_VkExtensions.Clear();

	//destroying old frames array (theoretically, the if is useless as free(nullptr) does nothing, but it apparently crashes on some OS)
	_VulkanBackBuffers.Clear();
	VK_CLEAR_ARRAY(_VulkanDepthBuffers, _nb_vk_frames, vkDestroyImage, _VulkanDevice);
	vkFreeMemory(_VulkanDevice, _VulkanDepthBufferMemory, nullptr);
	//destroying old framesbuffers associated with frames
	VK_CLEAR_ARRAY(_VulkanBackColourBuffers, _nb_vk_frames, vkDestroyImageView, _VulkanDevice);
	VK_CLEAR_ARRAY(_VulkanDepthBufferViews, _nb_vk_frames, vkDestroyImageView, _VulkanDevice);
	//destroy semaphores
	VK_CLEAR_ARRAY(_RuntimeHandle._VulkanCanPresentSemaphore, _nb_vk_frames, vkDestroySemaphore, _VulkanDevice);
	VK_CLEAR_ARRAY(_RuntimeHandle._VulkanHasPresentedSemaphore, _nb_vk_frames, vkDestroySemaphore, _VulkanDevice);
	//destroy fence
	VK_CLEAR_ARRAY(_RuntimeHandle._VulkanIsDrawingFence, _nb_vk_frames, vkDestroyFence, _VulkanDevice);
	//destroy command buffers (we want the same number of command buffer as backbuffers)
	if (_RuntimeHandle._VulkanCommand != nullptr)
	{
		vkFreeCommandBuffers(_VulkanDevice, _VulkanCommandPool[0], _nb_vk_frames, *_RuntimeHandle._VulkanCommand);
		_RuntimeHandle._VulkanCommand.Clear();
	}

	//destroy command pools
	if (_VulkanCommandPool[0])
		vkDestroyCommandPool(_VulkanDevice, _VulkanCommandPool[0], nullptr);
	if (_VulkanCommandPool[1])
		vkDestroyCommandPool(_VulkanDevice, _VulkanCommandPool[1], nullptr);


	vkDestroySwapchainKHR(_VulkanDevice, _VulkanSwapchain, nullptr);
	vkDestroyDevice(_VulkanDevice, nullptr);
	vkDestroySurfaceKHR(_VulkanInterface, _VulkanSurface, nullptr);
	vkDestroyInstance(_VulkanInterface, nullptr);

	glfwDestroyWindow(_VulkanWindow);
}


/*===== Graphics API Support =====*/

bool GraphicsAPIManager::FindAPISupported()
{
	/* Vulkan Support */

	//variable used to gauge if a vulkan call went wrong.
	VkResult result = VK_SUCCESS;

	//work as already been done so we'll piggy back on glfw method to find out if we Vulkan is supported
	//then for each scene we'll check on scene by scene basis.
	_vulkan_supported = glfwVulkanSupported();

	//first, recover the supported extension count of this machine
	VK_CALL_PRINT(vkEnumerateInstanceExtensionProperties(nullptr, &_vk_extension_count, nullptr));
	//allocate the space needed to retrieve all the extensions.
	_VkExtensions.Alloc(_vk_extension_count);

	printf("\nVulkan extension supported %d. below available extensions:\n", _vk_extension_count);

	//retrieve all the extensions
	VK_CALL_PRINT(vkEnumerateInstanceExtensionProperties(nullptr, &_vk_extension_count, *_VkExtensions));

	for (uint32_t i = 0; i < _vk_extension_count; i++)
	{
		printf("%s.\n", _VkExtensions[i].extensionName);
	}

	/* DX12 Support */

	/* Metal Support */

	return _vulkan_supported ||  _DX12_supported || _metal_supported;
}

bool GraphicsAPIManager::FindVulkanRTSupported(const MultipleVolatileMemory<VkExtensionProperties>& PhysicalDeviceExtensionProperties, uint32_t physicalExtensionNb)
{
	//used to count how many of the extensions we need this machine supports
	char needed_extensions = 0;
	//sadly extremely ineffective but necessary look up of all the extensions to see if we support raytracing
	for (uint32_t i = 0; i < physicalExtensionNb; i++)
	{
		if (strcmp(PhysicalDeviceExtensionProperties[i].extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0
			|| strcmp(PhysicalDeviceExtensionProperties[i].extensionName, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) == 0
			|| strcmp(PhysicalDeviceExtensionProperties[i].extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0)
		{
			if (++needed_extensions >= 3)
				break;
		}
	}
	_vulkan_rt_supported |= needed_extensions >= 3;

	//finding out if necessary extensions are supported. theoritically
	return needed_extensions >= 3;
}

bool GraphicsAPIManager::FindRTSupported()
{
	return _vulkan_rt_supported || _DXR_supported || _metal_rt_supported;
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

#ifdef __APPLE__
    //create a new variable to add the extensions we want other than those glfw needs
	MultipleScopedMemory<const char*> allExtensions{ glfwExtensionCount + 2 };
    allExtensions[glfwExtensionCount + 1] = "VK_KHR_portability_enumeration";
	createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	createInfo.enabledExtensionCount = glfwExtensionCount + 2;
#else
    //create a new variable to add the extensions we want other than those glfw needs
	MultipleScopedMemory<const char*> allExtensions{ glfwExtensionCount + 1 };
	createInfo.enabledExtensionCount = glfwExtensionCount + 1;
#endif
	createInfo.ppEnabledExtensionNames = *allExtensions;


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
	MultipleVolatileMemory<VkLayerProperties> layers{ (VkLayerProperties*)alloca(layer_count * sizeof(VkLayerProperties)) };
	VK_CALL_PRINT(vkEnumerateInstanceLayerProperties(&layer_count, *layers))

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
#endif

	//fill the create struct

	printf("\nEnabled Extensions:\n");
	for (uint32_t i = 0; i < glfwExtensionCount + 1; i++)
	{
		printf("%s.\n", allExtensions[i]);
	}

	//creating the instance
	VK_CALL_PRINT(vkCreateInstance(&createInfo, nullptr, &_VulkanInterface));
	return result == VK_SUCCESS;
}

bool GraphicsAPIManager::CreateGraphicsInterfaces()
{
	CreateVulkanInterface();

	return _VulkanInterface != nullptr;
}

bool GraphicsAPIManager::CreateVulkanHardwareInterface()
{
	//variable used to gauge if a vulkan call went wrong.
	VkResult result = VK_SUCCESS;

	// get the number of physical device
	uint32_t deviceCount = 0;
	VK_CALL_PRINT(vkEnumeratePhysicalDevices(_VulkanInterface, &deviceCount, nullptr))

	//the list of physical devices currently available for Vulkan
	MultipleVolatileMemory<VkPhysicalDevice> devices{ (VkPhysicalDevice*)alloca(deviceCount * sizeof(VkPhysicalDevice)) };
	VK_CALL_PRINT(vkEnumeratePhysicalDevices(_VulkanInterface, &deviceCount, *devices))

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
		MultipleVolatileMemory<VkExtensionProperties> GPUExtensions{ (VkExtensionProperties*)alloca(sizeof(VkExtensionProperties) * deviceExtensionNb) };
		VK_CALL_PRINT(vkEnumerateDeviceExtensionProperties(devices[i], nullptr, &deviceExtensionNb, *GPUExtensions));

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

	_VulkanGPU = devices[currentDeviceID];

	//the number of queue family this GPU is capable of
	//(this entails being able to run compute shader, or if the GPU has video decode/encode cores)
	//Raytracing is considered as a graphic queue, same as rasterizer.
	uint32_t queueFamilyNb = 0;
	vkGetPhysicalDeviceQueueFamilyProperties2(_VulkanGPU, &queueFamilyNb, nullptr);

	//the queue familty this GPU supports
	MultipleVolatileMemory<VkQueueFamilyProperties> queueFamilyProperties{ (VkQueueFamilyProperties*)alloca(sizeof(VkQueueFamilyProperties) * queueFamilyNb) };
	memset(*queueFamilyProperties, 0, sizeof(VkQueueFamilyProperties) * queueFamilyNb);
	vkGetPhysicalDeviceQueueFamilyProperties(_VulkanGPU, &queueFamilyNb, *queueFamilyProperties);

	for (; _vk_queue_family <= queueFamilyNb; _vk_queue_family++)
	{
		if (_vk_queue_family == queueFamilyNb)
		{
			queueFamilyNb = 0;
			break;
		}

		if (queueFamilyProperties[_vk_queue_family].queueFlags &
			(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
			break;
	}
	
	//create the queues associated with the device
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = _vk_queue_family = queueFamilyNb == 0 ? 0 : _vk_queue_family;
	float queuePriorities[2] = { 0.0f, 1.0f };
	queueCreateInfo.pQueuePriorities = queuePriorities;
	queueCreateInfo.queueCount = 2;

	//create the device with the proper extension
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.queueCreateInfoCount = 1;
	

	//if we can do raytracing, we need to activate the feature
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracingFeatures{};
	raytracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	raytracingFeatures.rayTracingPipeline = _vulkan_rt_supported;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelerationStructureFeatures.accelerationStructure = _vulkan_rt_supported;
	accelerationStructureFeatures.pNext = &raytracingFeatures;

	//raytracing means we are on vulan1.3 (as GPU raytracing was introduced in 1.3), so 1.2 is given. still activating basic feature needed for raytracing.
	VkPhysicalDeviceVulkan12Features vulkan12Features{};
	vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vulkan12Features.bufferDeviceAddress = _vulkan_rt_supported;
	vulkan12Features.pNext = &accelerationStructureFeatures;

	deviceCreateInfo.pNext = &vulkan12Features;



#ifdef __APPLE__
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset",  VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME };
#else
	const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
#endif
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	if (_vulkan_rt_supported)
	{
		deviceCreateInfo.enabledExtensionCount = 5;
	}
	else
	{
		deviceCreateInfo.enabledExtensionCount = 1;
	}

#ifdef __APPLE__
    deviceCreateInfo.enabledExtensionCount++;
#endif

	//Vulkan Physical Device chosen
	VK_CALL_PRINT(vkCreateDevice(_VulkanGPU, &deviceCreateInfo, nullptr, &_VulkanDevice));
	_RuntimeHandle._VulkanDevice = _VulkanDevice;

	//announce what GPU we end up with
	{
		VkPhysicalDeviceProperties2 deviceProperty{};
		deviceProperty.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

		vkGetPhysicalDeviceProperties2(_VulkanGPU, &deviceProperty);

		printf("Selected %d - %s. Raytracing Supports : %s.\n", currentDeviceID, deviceProperty.properties.deviceName, currentRaytracingSupported ? "true" : "false");
	}

	//Getting back the first queue
	VkDeviceQueueInfo2 QueueInfo{};
	QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	vkGetDeviceQueue2(_VulkanDevice, &QueueInfo, &_RuntimeHandle._VulkanQueues[0]);

	//getting back the second queue
	QueueInfo.queueIndex = 1;
	vkGetDeviceQueue2(_VulkanDevice, &QueueInfo, &_RuntimeHandle._VulkanQueues[1]);

	{
		//create command pool objects
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = _vk_queue_family;

		//making the first command pool
		VK_CALL_PRINT(vkCreateCommandPool(_VulkanDevice, &poolInfo, nullptr, &_VulkanCommandPool[0]));
		//making the second command pool
		VK_CALL_PRINT(vkCreateCommandPool(_VulkanDevice, &poolInfo, nullptr, &_VulkanCommandPool[1]));
	}

	//load the raytracing funtion if we have a capable GPU
	if (_vulkan_rt_supported)
	{
	}

	return true;
}

bool GraphicsAPIManager::CreateHardwareInterfaces()
{
	CreateVulkanHardwareInterface();

	return _VulkanDevice != nullptr;
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

	if (_vulkan_supported && _vulkan_rt_supported)
		window_name = "Vulkan w/ RT\0";
	else if (_vulkan_supported)
		window_name = "Vulkan No RT\0";

	if (_vulkan_supported)
	{
		//creating a window. size is quite arbitrary
		_VulkanWindow = glfwCreateWindow(1080, 600, window_name, nullptr, nullptr);

		VkResult result = VK_SUCCESS;
		VK_CALL_PRINT(glfwCreateWindowSurface(_VulkanInterface, _VulkanWindow, nullptr, &_VulkanSurface));

		uint32_t formatCount{0};
		vkGetPhysicalDeviceSurfaceFormatsKHR(_VulkanGPU, _VulkanSurface, &formatCount, nullptr);

		//there shouldn't be that much format, put it on stack
		MultipleVolatileMemory<VkSurfaceFormatKHR> formats{ (VkSurfaceFormatKHR*)alloca(sizeof(VkSurfaceFormatKHR) * formatCount) };
		vkGetPhysicalDeviceSurfaceFormatsKHR(_VulkanGPU, _VulkanSurface, &formatCount, *formats);
		for (uint32_t i = 0; i < formatCount; i++)
		{
			if (formats[i].format == VK_FORMAT_R8G8B8A8_SRGB || formats[i].format == VK_FORMAT_B8G8R8A8_SRGB)
			{
				_VulkanSurfaceFormat = formats[i];
				break;
			}
		}

		glfwGetFramebufferSize(_VulkanWindow, &_vk_width, &_vk_height);
		ResizeVulkanSwapChain(_vk_width, _vk_height);

		//window creation was successful, let's get surface info so that we'll be able to use it when needing to recreate the framebuffer
		//in case of resize for example.
		if (result == VK_SUCCESS)
		{
			nb_window_init++;
		}
	}

	/* DirectX Support */

	if (_DX12_supported && _DXR_supported)
		window_name = "DirectX 12 w/ DXR\0";
	else if (_DX12_supported)
		window_name = "DirectX 12 no DXR\0";

	if (_DX12_supported)
	{
		//windows[nb_window_init++] = glfwCreateWindow(800, 600, window_name, nullptr, nullptr);
	}

	/* Metal Support */


	if (_metal_supported && _metal_rt_supported)
		window_name = "Metal w/ RT\0";
	else if (_metal_supported)
		window_name = "Metal no RT\0";

	if (_metal_supported)
	{
		//windows[nb_window_init++] = glfwCreateWindow(800, 600, window_name, nullptr, nullptr);
	}

	return nb_window_init > 0;
}

bool GraphicsAPIManager::ResizeVulkanSwapChain(int32_t width, int32_t height)
{
	vkDeviceWaitIdle(_VulkanDevice);

	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	//clear previous resources
	{

		//destroying old frames array (theoretically, the if is useless as free(nullptr) does nothing, but it apparently crashes on some OS)
		_VulkanBackBuffers.Clear();
		VK_CLEAR_ARRAY(_VulkanDepthBuffers, _nb_vk_frames, vkDestroyImage, _VulkanDevice);
		vkFreeMemory(_VulkanDevice, _VulkanDepthBufferMemory, nullptr);
		//destroying old framesbuffers associated with frames
		VK_CLEAR_ARRAY(_VulkanBackColourBuffers, _nb_vk_frames, vkDestroyImageView, _VulkanDevice);
		VK_CLEAR_ARRAY(_VulkanDepthBufferViews, _nb_vk_frames, vkDestroyImageView, _VulkanDevice);
		//destroy semaphores
		VK_CLEAR_ARRAY(_RuntimeHandle._VulkanCanPresentSemaphore, _nb_vk_frames, vkDestroySemaphore, _VulkanDevice);
		VK_CLEAR_ARRAY(_RuntimeHandle._VulkanHasPresentedSemaphore, _nb_vk_frames, vkDestroySemaphore, _VulkanDevice);
		//destroy fence
		VK_CLEAR_ARRAY(_RuntimeHandle._VulkanIsDrawingFence, _nb_vk_frames, vkDestroyFence, _VulkanDevice);
		//destroy command buffers (we want the same number of command buffer as backbuffers)
		if (_RuntimeHandle._VulkanCommand != nullptr)
		{
			vkFreeCommandBuffers(_VulkanDevice, _VulkanCommandPool[0], _nb_vk_frames, *_RuntimeHandle._VulkanCommand);
			_RuntimeHandle._VulkanCommand.Clear();
		}

	}

	//create or recreate swapchain
	{
		//the struct allowing us to know the display capabilities of our hardware
		VkSurfaceCapabilitiesKHR SurfaceCapabilities{};

		//gettting back our display capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_VulkanGPU, _VulkanSurface, &SurfaceCapabilities);

		VkSwapchainKHR tempSwapchain{ _VulkanSwapchain };

		//creating the swap chain
		VkSwapchainCreateInfoKHR createinfo{};
		createinfo.sType					= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createinfo.surface					= _VulkanSurface;
		createinfo.minImageCount			= SurfaceCapabilities.maxImageCount;
		createinfo.imageFormat				= _VulkanSurfaceFormat.format;
		createinfo.imageColorSpace			= _VulkanSurfaceFormat.colorSpace;
		createinfo.imageExtent				= VkExtent2D{ (uint32_t)width, (uint32_t)height };
		createinfo.imageUsage				= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createinfo.queueFamilyIndexCount	= _vk_queue_family;
		createinfo.imageArrayLayers			= 1;
		createinfo.compositeAlpha			= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createinfo.imageSharingMode			= VK_SHARING_MODE_EXCLUSIVE;
		createinfo.clipped					= VK_TRUE;
		createinfo.presentMode				= VK_PRESENT_MODE_MAILBOX_KHR;//upload as soon as available
		createinfo.oldSwapchain				= tempSwapchain;
		createinfo.preTransform				= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

		//creating the swapchain
		VK_CALL_PRINT(vkCreateSwapchainKHR(_VulkanDevice, &createinfo, nullptr, &_VulkanSwapchain))

		//destroying old
		vkDestroySwapchainKHR(_VulkanDevice, tempSwapchain, nullptr);
	}

	//creating back buffers
	{
		//get back the number of frames the swapchain was able to create
		VK_CALL_PRINT(vkGetSwapchainImagesKHR(_VulkanDevice, _VulkanSwapchain, &_nb_vk_frames, nullptr));
		_RuntimeHandle._nb_vk_frames = _nb_vk_frames;
		_vk_width = width;
		_vk_height = height;
		_VulkanBackBuffers.Alloc(_nb_vk_frames);
		// get back the actual frames
		VK_CALL_PRINT(vkGetSwapchainImagesKHR(_VulkanDevice, _VulkanSwapchain, &_nb_vk_frames, *_VulkanBackBuffers));

		//recreate the array of framebuffers associated with the swapchain's frames.
		_VulkanBackColourBuffers.Alloc(_nb_vk_frames);

		//describe the ImageView (FrameBuffers) associated with the Frames of the swap chain we want to create
		VkImageViewCreateInfo frameBufferInfo{};
		frameBufferInfo.sType							= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		frameBufferInfo.components						= { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		frameBufferInfo.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		frameBufferInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;
		frameBufferInfo.format							= _VulkanSurfaceFormat.format;
		frameBufferInfo.subresourceRange.baseMipLevel	= 0;
		frameBufferInfo.subresourceRange.levelCount		= 1;
		frameBufferInfo.subresourceRange.baseArrayLayer = 0;
		frameBufferInfo.subresourceRange.layerCount		= 1;

		for (uint32_t i = 0; i < _nb_vk_frames; i++)
		{
			frameBufferInfo.image = _VulkanBackBuffers[i];
			VK_CALL_PRINT(vkCreateImageView(_VulkanDevice, &frameBufferInfo, nullptr, &(_VulkanBackColourBuffers[i])));
		}
	}

	//create depth buffers
	{
		_VulkanDepthBuffers.Alloc(_nb_vk_frames);
		_VulkanDepthBufferViews.Alloc(_nb_vk_frames);

		VkImageCreateInfo depthBufferInfo{};
		depthBufferInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		depthBufferInfo.imageType		= VK_IMAGE_TYPE_2D;//can be 2D 
		depthBufferInfo.format			= VK_FORMAT_D32_SFLOAT;//for now we'll just do depth without stencil
		depthBufferInfo.extent.width	= width;
		depthBufferInfo.extent.height	= height;
		depthBufferInfo.extent.depth	= 1;
		depthBufferInfo.mipLevels		= 1u;
		depthBufferInfo.arrayLayers		= 1u;
		depthBufferInfo.tiling			= VK_IMAGE_TILING_OPTIMAL;//this we'll be wrote znyway so keep it as optimal
		depthBufferInfo.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;//no need to init
		depthBufferInfo.usage			= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;//same as above
		depthBufferInfo.sharingMode		= VK_SHARING_MODE_EXCLUSIVE;//we have to command queues : one for the app, one for the UI. the UI shouldn't use the depth buffer.
		depthBufferInfo.samples			= VK_SAMPLE_COUNT_1_BIT;//why would you want more than one sample in a simple app ?
		depthBufferInfo.flags = 0; // Optional

		VK_CALL_PRINT(vkCreateImage(_VulkanDevice, &depthBufferInfo, nullptr, &_VulkanDepthBuffers[0]));

		//getting the necessary requirements to create our image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(_VulkanDevice, _VulkanDepthBuffers[0], &memRequirements);

		//trying to find a matching memory type between what the app wants and the device's limitation.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize	= memRequirements.size * _nb_vk_frames;
		vkGetPhysicalDeviceMemoryProperties(_VulkanGPU, &_VulkanUploader._MemoryProperties);
		allocInfo.memoryTypeIndex	= VulkanHelper::GetMemoryTypeFromRequirements(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements, _VulkanUploader._MemoryProperties);

		VK_CALL_PRINT(vkAllocateMemory(_VulkanDevice, &allocInfo, nullptr, &_VulkanDepthBufferMemory));

		//creating the image view fromn the image
		VkImageViewCreateInfo depthBufferViewInfo{};
		depthBufferViewInfo.sType							= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		depthBufferViewInfo.image							= _VulkanDepthBuffers[0];
		depthBufferViewInfo.components						= { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		depthBufferViewInfo.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_DEPTH_BIT;
		depthBufferViewInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;
		depthBufferViewInfo.format							= VK_FORMAT_D32_SFLOAT;
		depthBufferViewInfo.subresourceRange.baseMipLevel	= 0;
		depthBufferViewInfo.subresourceRange.levelCount		= 1;
		depthBufferViewInfo.subresourceRange.baseArrayLayer = 0;
		depthBufferViewInfo.subresourceRange.layerCount		= 1;

		VK_CALL_PRINT(vkBindImageMemory(_VulkanDevice, _VulkanDepthBuffers[0], _VulkanDepthBufferMemory, 0));
		VK_CALL_PRINT(vkCreateImageView(_VulkanDevice, &depthBufferViewInfo, nullptr, &_VulkanDepthBufferViews[0]));

		for (uint32_t i = 1; i < _nb_vk_frames; i++)
		{
			VK_CALL_PRINT(vkCreateImage(_VulkanDevice, &depthBufferInfo, nullptr, &_VulkanDepthBuffers[i]));
			VK_CALL_PRINT(vkBindImageMemory(_VulkanDevice, _VulkanDepthBuffers[i], _VulkanDepthBufferMemory, i* memRequirements.size));
			depthBufferViewInfo.image = _VulkanDepthBuffers[i];
			VK_CALL_PRINT(vkCreateImageView(_VulkanDevice, &depthBufferViewInfo, nullptr, &_VulkanDepthBufferViews[i]));
		}
	}


	//create semaphores and fences
	{

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		//allocate for new semaphores

		_RuntimeHandle._VulkanCanPresentSemaphore.Alloc(_nb_vk_frames);
		_RuntimeHandle._VulkanHasPresentedSemaphore.Alloc(_nb_vk_frames);
		_RuntimeHandle._VulkanIsDrawingFence.Alloc(_nb_vk_frames);

		for (uint32_t i = 0; i < _nb_vk_frames; i++)
		{
			VK_CALL_PRINT(vkCreateSemaphore(_VulkanDevice, &semaphoreInfo, nullptr, &_RuntimeHandle._VulkanCanPresentSemaphore[i]));
			VK_CALL_PRINT(vkCreateSemaphore(_VulkanDevice, &semaphoreInfo, nullptr, &_RuntimeHandle._VulkanHasPresentedSemaphore[i]));
			VK_CALL_PRINT(vkCreateFence(_VulkanDevice, &fenceInfo, nullptr, &_RuntimeHandle._VulkanIsDrawingFence[i]));
		}

	}

	//create commmand buffers
	{

		//first allocate
		_RuntimeHandle._VulkanCommand.Alloc(_nb_vk_frames);

		//then fill up the command buffers in the array
		VkCommandBufferAllocateInfo command_info{};
		command_info.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		command_info.commandPool		= _VulkanCommandPool[0];
		command_info.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		command_info.commandBufferCount = _nb_vk_frames;

		vkAllocateCommandBuffers(_VulkanDevice, &command_info, *_RuntimeHandle._VulkanCommand);
	}

	return result == VK_SUCCESS;
}

bool GraphicsAPIManager::ResizeSwapChain(ScopedLoopArray<Scene*>& SceneToChange)
{
	/* Vulkan Support */

	//may seem strange but the Graphics API is in charge of resizing the scenes
	//as implementation may require different stuff between API
	if (_vulkan_supported)
	{
		int32_t old_width		= _vk_width;
		int32_t old_height		= _vk_height;
		uint32_t oldNbFrames	= _nb_vk_frames;

		//gettign the new size of window (it may not have changed)
		glfwGetFramebufferSize(_VulkanWindow, &_vk_width, &_vk_height);

		//first we resize window and get all back buffers
		bool toReturn = ResizeVulkanSwapChain(_vk_width, _vk_height);

		//We use a temporary uploader struct in order to avoid leaks and destroy all temporary data on GPU at once
		PrepareForUpload();

		//we only resize scenes if we succeeded in resizing the window
		if (toReturn)
			for (uint32_t i = 0; i < SceneToChange.Nb(); i++)
				SceneToChange[i]->Resize(*this, old_width, old_height, oldNbFrames);

		//submit all the resize work to GPU
		SubmitUpload();

		return toReturn;

	}

	return false;
}


bool GraphicsAPIManager::PrepareForUpload()
{
	bool success = true;

	/* vulkan */
	success &= VulkanHelper::StartUploader(*this, _VulkanUploader);

	return success;
}


bool GraphicsAPIManager::SubmitUpload()
{
	bool success = true;

	/* vulkan */
	success &= VulkanHelper::SubmitUploader(_VulkanUploader);

	return success;
}

/*===== Graphics API Queues and Command =====*/

bool GraphicsAPIManager::GetVulkanNextFrame()
{
	VkResult result = VK_SUCCESS;

	{
		//if we already looped through all the frames in our swapchain, but this one did not finish drawing, we have no other choice than wait
		vkWaitForFences(_VulkanDevice, 1, &_RuntimeHandle._VulkanIsDrawingFence[_RuntimeHandle._vk_current_frame], VK_TRUE, UINT64_MAX);

		//if we did not wait or
		vkResetFences(_VulkanDevice, 1, &_RuntimeHandle._VulkanIsDrawingFence[_RuntimeHandle._vk_current_frame]);
	}

	VK_CALL_PRINT(vkAcquireNextImageKHR(_VulkanDevice, _VulkanSwapchain, UINT64_MAX, _RuntimeHandle._VulkanCanPresentSemaphore[_RuntimeHandle._vk_current_frame], VK_NULL_HANDLE, &_RuntimeHandle._vk_frame_index));

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		return false;

	int32_t width, height {0};

	glfwGetFramebufferSize(_VulkanWindow, &width, &height);

	return _vk_width == width && _vk_height == height;
}

bool GraphicsAPIManager::GetNextFrame()
{
	return GetVulkanNextFrame();
}
