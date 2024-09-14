#ifndef __VULKAN_HELPER_H__
#define __VULKAN_HELPER_H__

#include "ConcatenatedVulkan.h"

/* forward def */
class GraphicsAPIManager;

/* Buffers */

/**
* A struct used to create the necessary handles and pointers for a glsl shader's "uniform buffer".
*/
struct UniformBufferHandle
{
	VkBuffer		GPUBuffer;
	VkDeviceMemory	GPUMemoryHandle;
	void*			CPUMemoryHandle{nullptr};
};

/* Creates a one dimensionnal buffer of any usage and the association between CPU and GPU */
bool CreateVulkanBuffer(GraphicsAPIManager& GAPI, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

/* Creates all the pointers and handle vulkan needs to create a buffer on the GPU and creates a UniformBufferHandle on the CPU to manage it*/
bool CreateUniformBufferHandle(GraphicsAPIManager& GAPI, UniformBufferHandle& bufferHandle, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, bool mapCPUMemoryHandle = true);


#endif //__VULKAN_HELPER_H__