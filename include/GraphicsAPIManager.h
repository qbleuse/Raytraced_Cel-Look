#ifndef __GRAPHICSAPIMANAGER_H__
#define __GRAPHICSAPIMANAGER_H__

#include <cstdint>

#include "VulkanHelper.h"
#include "Utilities.h"
#include "vulkan/vulkan_core.h"


struct GAPIHandle
{
	/* Vulkan */
	VkDevice								_VulkanDevice;
	VkQueue									_VulkanQueues[2]{ VK_NULL_HANDLE, VK_NULL_HANDLE };
	MultipleScopedMemory<VkCommandBuffer>	_VulkanCommand;
	MultipleScopedMemory<VkSemaphore>		_VulkanCanPresentSemaphore;
	MultipleScopedMemory<VkSemaphore>		_VulkanHasPresentedSemaphore;
	MultipleScopedMemory<VkFence>			_VulkanIsDrawingFence;

	uint32_t _vk_current_frame = 0;
	uint32_t _vk_frame_index = 0;
	uint32_t _nb_vk_frames{ 0 };

	/*
	* Returns the command buffer currently used for this frame to record commands
	*/
	__forceinline const VkCommandBuffer& GetCurrentVulkanCommand()const noexcept { return _VulkanCommand[_vk_current_frame]; }

	/*
	* Returns the semaphore currently used for this frame to track if we can preesent
	*/
	__forceinline const VkSemaphore& GetCurrentCanPresentSemaphore()const noexcept { return _VulkanCanPresentSemaphore[_vk_current_frame]; }

	/*
	* Returns the semaphore currently used for this frame to track if frame was presented
	*/
	__forceinline const VkSemaphore& GetCurrentHasPresentedSemaphore()const noexcept { return _VulkanHasPresentedSemaphore[_vk_current_frame]; }

	/*
	* Returns the fence currently used for this frame to track if GPU is still executing command
	*/
	__forceinline const VkFence& GetCurrentIsDrawingFence()const noexcept { return _VulkanIsDrawingFence[_vk_current_frame]; }

};

/**
* class that manages what Graphics API this machine supports, and initialize the associated resources.
* For now only Vulkan is supported by the application.
* This application has the purpose of only supporting "new" Graphics API (Vulkan, DirectX, Metal)
*/
class GraphicsAPIManager
{
#pragma region Graphics API Manager
	/*===== GRAPHICS API MANAGER =====*/
public:

	~GraphicsAPIManager();

	GAPIHandle				_RuntimeHandle;

	/*===== END GRAPHICS API MANAGER =====*/
#pragma endregion

#pragma region Graphics API Support
	/*===== GRAPHICS API SUPPORT =====*/
public:

		/* Vulkan */

		//whether this machine supports Vulkan
		bool _vulkan_supported = false;
		//whether this machine supports hardware accelerated Raytracing through Vulkan
		bool _vulkan_rt_supported = false;
		//the number of supported extension that vulkan has
		uint32_t _vk_extension_count = 0;
		//the vulkan is supported, the array of extension supported by the current machine.
		//this will be a C-array that you will be able to go through usin vk_extension_count.
		MultipleScopedMemory<struct VkExtensionProperties> _VkExtensions;
		//an helper object to upload resources to GPU
		VulkanHelper::Uploader _VulkanUploader;

		/**
		* Finds if raytracing extensions is supported by the Vulkan on the specified Physical Device.
		* /!\ this will change the value of the above flags accordingly as if there is a device that handles raytracing, it should be choosed over any other device.
		* - returns : whether or not raytracing is supported by this physical device.
		*/
		bool FindVulkanRTSupported(const MultipleVolatileMemory<struct VkExtensionProperties>& PhysicalDeviceExtensionProperties, uint32_t physicalExtensionNb);

		/* DirectX*/

		//whether this machine supports DirectX 12
		bool _DX12_supported = false;
		//whether this machine supports hardware accelerated Raytracing through DirectX Raytracing
		bool _DXR_supported = false;

		/* Metal */

		//whether this machine supports Metal
		bool _metal_supported = false;
		//whether this machine supports hardware accelerated Raytracing through Metal Raytracing
		//(for this at least a M3 machine would be necessary so not sure if this will really be implemented or not)
		bool _metal_rt_supported = false;

		/* Agnostic */

		/**
		* Find which Graphics API is supported by the machine. will change the value of the above flags accordingly to be used freely outside this class.
		* - returns : whether or not at least one graphics API is supported.
		*/
		bool FindAPISupported();

		/**
		* Finds if raytracing extensions is supported by the Graphics API on the machine. will change the value of the above flags accordingly to be used freely outside this class.
		* - returns : whether or not at least one graphics API is supported.
		*/
		bool FindRTSupported();

	/*===== END GRAPHICS API SUPPORT =====*/
#pragma endregion

#pragma region Graphics API Window and Device
	/*===== GRAPHICS API WINDOW AND DEVICE =====*/
public:

		/* Vulkan */

		//The Interface allowing the creation of application wide resources
		VkInstance			_VulkanInterface {VK_NULL_HANDLE};
		//The Interface that represents the chosen GPU
		VkPhysicalDevice	_VulkanGPU{ VK_NULL_HANDLE };
		//The Interface allowing the creation of hardware resources (usually tied to GPU)
		VkDevice			_VulkanDevice{ VK_NULL_HANDLE };
		//The Interface for presenting to screen
		VkSurfaceKHR		_VulkanSurface{VK_NULL_HANDLE};
		//The Interface to control the swap chain
		VkSwapchainKHR		_VulkanSwapchain{ VK_NULL_HANDLE };
		//The chosen format of the framebuffers
		VkSurfaceFormatKHR	_VulkanSurfaceFormat{};
		//the chosen present mode of the swapchain
		VkPresentModeKHR    _VulkanPresentMode{};

		//The actual frames of the swapchain. this is an array containing a number of VkImage (with a minimum of 3)
		MultipleScopedMemory<VkImage>		_VulkanBackBuffers;
		//The actual frames buffers of the swapchain. this is an array containing a number of VkImageViews equal to the number of frames.
		//This is the interface that will allow us to write into the actual frames we want to present to the screen.
		MultipleScopedMemory<VkImageView>	_VulkanBackColourBuffers;
		//The depth buffers. this is an array containing a number of VkImage (with a minimum of 3)
		MultipleScopedMemory<VkImage>		_VulkanDepthBuffers;
		//memory where all depth buffer will be allocated.
		VkDeviceMemory						_VulkanDepthBufferMemory{VK_NULL_HANDLE};
		//the view object for the depth buffers
		MultipleScopedMemory<VkImageView>	_VulkanDepthBufferViews;
		//The nb of actual Vulkan framebuffers in the Vulkan swapchain at any given time (this can change between hardware)
		uint32_t							_nb_vk_frames{ 0 };

		//The window linked to Vulkan
		struct GLFWwindow* _VulkanWindow{nullptr};
		int32_t _vk_width{ 0 };
		int32_t _vk_height{ 0 };

		/**
		* Initializes the VkInstance inside this class
		* - returns : whether VkInstance creation was a success.
		*/
		bool CreateVulkanInterface();

		/**
		* Initializes the VkPhysicalDevice of this class. This will only choose a single Physical Device, with preferably raytracing support.
		* Dedicated Graphics are prefered over integrated graphics.
		* - returns : whether VkPhysicalDevice creation was a success.
		*/
		bool CreateVulkanHardwareInterface();

		/**
		* Creates or recreates the VkSwapchainKHR of this class, using VulkanSurface.
		* This shoulbe use only after windows are created, as this need a surface.
		* - returns : whether Swapchain creation succeeded
		*/
		bool ResizeVulkanSwapChain(int32_t width, int32_t height);


		/* Agnostic */

		/**
		* Creates the associated Interface Object for each Graphics API supported.
		* - returns : whether at least one Interface was successfully created.
		*/
		bool CreateGraphicsInterfaces();

		/**
		* Creates the associated Hardware Interface Object for each Graphics API supported.
		* This will only choose a single Physical Device per Graphics API, with preferably raytracing support.
		* Dedicated Graphics are prefered over integrated graphics.
		* - returns : whether at least one Hardware Interface was successfully created.
		*/
		bool CreateHardwareInterfaces();

		/**
		* Create A GLFWwindow struct for each Graphics API supported.
		* max that can be supported for one machine is two. (2 windows expected on Windows, one on Linux and two on MacOS).
		* - returns : whether at least one window was successfully created.
		*
		*/
		bool MakeWindows();

		/**
		* Creates or recreates a swapchain associated with the window given in parameter.
		* - returns : whether at least one window was successfully created.
		*
		*/
		bool ResizeSwapChain();

		/**
		* Creates any platform specific interface needed to upload resources to GPU
		* - returns : if interfaces were created
		*/
		bool PrepareForUpload();

		/**
		* Upload resources to GPU
		* - returns : submit was launched (/!\ we don't know if it was successful /!\)
		*/
		bool SubmitUpload();




	/*===== END GRAPHICS API WINDOW AND DEVICE  =====*/
#pragma endregion


#pragma region Graphics API Queue And CommandLists
/*===== GRAPHICS API QUEUES AND COMMAND =====*/
	public:

		/* Vulkan */

		//The Interface allowing the creation of Command to the chosen physical device
		uint32_t		_vk_queue_family{ 0 };
		VkCommandPool	_VulkanCommandPool[2]{ VK_NULL_HANDLE, VK_NULL_HANDLE };

		/*
		* Gets the index of Vulkan's swapchain framebuffer that can be used.
		* - returns : if current index could be get, false means VK_ERROR_OUT_OF_DATE_KHR or VK_SUBOPTIMAL_KHR.
		*/
		bool GetVulkanNextFrame();


		/* Agnostic */

		/*
		* Gets the index of each Graphics API swapchain framebuffer that can be used.
		* - returns : if current index could be get, false usually means needing to recreate swapchain.
		*/
		bool GetNextFrame();



		/*===== END GRAPHICS API QUEUE AND Command Lists=====*/
#pragma endregion

};

#endif // !__GRAPHICSAPIMANAGER_H__
