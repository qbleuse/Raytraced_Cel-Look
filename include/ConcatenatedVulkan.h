#ifndef __CONCATENATED_VULKAN_H__
#define __CONCATENATED_VULKAN_H__

#include <vulkan/vulkan.h>

//vulkan include
#include <vulkan/vk_enum_string_helper.h>

#define VK_CALL_PRINT(vk_call)\
	result = vk_call;\
	if (result != VK_SUCCESS)\
	{\
		printf("Vulkan call error : %s ; result : %s .\n", #vk_call, string_VkResult(result));\
	}\

#endif //__CONCATENATED_VULKAN_H__