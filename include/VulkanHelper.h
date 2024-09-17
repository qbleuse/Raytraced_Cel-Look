#ifndef __VULKAN_HELPER_H__
#define __VULKAN_HELPER_H__


//vulkan include
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "Utilities.h"


/* defines */

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




/* forward def */
class GraphicsAPIManager;


/* Shaders */

bool CreateVulkanShaders(GraphicsAPIManager& GAPI, VkShaderModule& shader, VkShaderStageFlagBits shaderStage, const char* shaderSource, const char* shader_name, const char* entryPoint = "main");

/* Buffers */

/**
* A struct in struct of array format used to create the necessary handles and pointers for a glsl shader's "uniform buffer", for each frame in flight.
*/
struct UniformBufferHandle
{
	SimpleArray<VkBuffer>		GPUBuffer;
	SimpleArray<VkDeviceMemory>	GPUMemoryHandle;
	SimpleArray<void*>			CPUMemoryHandle;
};

/* Creates a one dimensionnal buffer of any usage and the association between CPU and GPU */
bool CreateVulkanBuffer(GraphicsAPIManager& GAPI, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);


/* Creates all the pointers and handle vulkan needs to create a buffer on the GPU and creates a UniformBufferHandle on the CPU to manage it*/
bool CreateUniformBufferHandle(GraphicsAPIManager& GAPI, UniformBufferHandle& bufferHandle, uint32_t bufferNb, VkDeviceSize size, 
	VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,  bool mapCPUMemoryHandle = true);

/* Asks for de-allocation of all allocated resources for this UniformBufferHandle, on CPU and GPU*/
void ClearUniformBufferHandle(GraphicsAPIManager& GAPI, UniformBufferHandle& bufferHandle);


#endif //__VULKAN_HELPER_H__