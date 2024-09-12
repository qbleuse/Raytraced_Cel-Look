#ifndef __VULKAN_HELPER_H__
#define __VULKAN_HELPER_H__

#include "ConcatenatedVulkan.h"

/* forward def */
class GraphicsAPIManager;

/* Buffers */

/* Creates a one dimensionnal buffer of any usage and the association between CPU and GPU */
bool CreateVulkanBuffer(GraphicsAPIManager& GAPI, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory);


#endif //__VULKAN_HELPER_H__