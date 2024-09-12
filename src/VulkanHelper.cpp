#include "VulkanHelper.h"

#include "GraphicsAPIManager.h"

/* BUFFERS */

bool CreateVulkanBuffer(GraphicsAPIManager& GAPI, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory)
{
	VkResult result = VK_SUCCESS;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CALL_PRINT(vkCreateBuffer(GAPI.VulkanDevice, &bufferInfo, nullptr, buffer))
	
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(GAPI.VulkanDevice, *buffer, &memRequirements);
	
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = size;
	
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(GAPI.VulkanGPU, &memProperties);
	
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (memRequirements.memoryTypeBits & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			allocInfo.memoryTypeIndex = i;
			break;
		}
	}

	VK_CALL_PRINT(vkAllocateMemory(GAPI.VulkanDevice, &allocInfo, nullptr, bufferMemory))

	VK_CALL_PRINT(vkBindBufferMemory(GAPI.VulkanDevice, *buffer, *bufferMemory, 0));

	return result == VK_SUCCESS;
}