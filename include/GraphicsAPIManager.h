#ifndef __GRAPHICSAPIMANAGER_H__
#define __GRAPHICSAPIMANAGER_H__

#include <cstdint>
#include <vulkan/vulkan.h>

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

	/*===== END GRAPHICS API MANAGER =====*/
#pragma endregion

#pragma region Graphics API Support
	/*===== GRAPHICS API SUPPORT =====*/
	public:

		/* Vulkan */

		//whether this machine supports Vulkan
		bool vulkan_supported = false;
		//whether this machine supports hardware accelerated Raytracing through Vulkan
		bool vulkan_rt_supported = false;
		//the number of supported extension that vulkan has
		uint32_t vk_extension_count = 0;
		//the vulkan is supported, the array of extension supported by the current machine.
		//this will be a C-array that you will be able to go through usin vk_extension_count.
		struct VkExtensionProperties* vk_extensions = nullptr;

		/* DirectX*/

		//whether this machine supports DirectX 12
		bool DX12_supported = false;
		//whether this machine supports hardware accelerated Raytracing through DirectX Raytracing
		bool DXR_supported = false;

		/* Metal */

		//whether this machine supports Metal
		bool metal_supported = false;
		//whether this machine supports hardware accelerated Raytracing through Metal Raytracing 
		//(for this at least a M3 machine would be necessary so not sure if this will really be implemented or not)
		bool metal_rt_supported = false;

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

		VkInstance VulkanInterface;

		/**
		* Initializes the VkInstance inside this class
		* - returns : wether VkInstanceCreation was a success.
		*/
		bool CreateVulkanInterface();


		/* Agnostic */

		/**
		* Creates the associated Interface Object for each Graphics API supported.
		* - returns : whether at least one Interface was successfully created.
		*/
		bool CreateGraphicsInterfaces();

		/**
		* Create A GLFWwindow struct for each Graphics API supported and add it to the GLFWwindow array given in parameter.
		* max that can be supported for one machine is two. (2 windows expected on Windows, one on Linux and two on MacOS).
		* - returns : whether at least one window was successfully created.
		*
		*/
		bool MakeWindows(struct GLFWwindow**)const;


	/*===== END GRAPHICS API WINDOW AND DEVICE  =====*/
#pragma endregion

};


#endif // !__GRAPHICSAPIMANAGER_H__
