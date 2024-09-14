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

#define VK_CLEAR_RAW_ARRAY(raw_array, size, call, device) \
	if (raw_array != nullptr)\
	{\
		for (uint32_t i = 0; i < size; i++)\
			call(device, raw_array[i], nullptr);\
		free(raw_array);\
	}\

#define VK_CLEAR_ARRAY(_array, size, call, device) \
	if (_array != nullptr)\
	{\
		for (uint32_t i = 0; i < size; i++)\
			call(device, _array[i], nullptr);\
		_array.Clear();\
	}\

#endif //__CONCATENATED_VULKAN_H__