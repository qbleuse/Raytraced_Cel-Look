#include "VulkanHelper.h"

#include "GraphicsAPIManager.h"

//vulkan shader compiler
#include "shaderc/shaderc.h"

//loader include
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


/*===== SHADERS =====*/

bool CreateVulkanShaders(GraphicsAPIManager& GAPI, VkShaderModule& shader, VkShaderStageFlagBits shaderStage, const char* shaderSource, const char* shader_name, const char* entryPoint)
{
	//our shader compiler 
	shaderc_compiler_t shader_compiler = shaderc_compiler_initialize();

	//the compile option of our compiler
	shaderc_compile_options_t compile_options = shaderc_compile_options_initialize();

	//infer the shader kind from the Vulkan Stage bit
	shaderc_shader_kind shader_kind = shaderc_glsl_infer_from_source;
	switch (shaderStage)
	{
	case VK_SHADER_STAGE_VERTEX_BIT:
		shader_kind = shaderc_vertex_shader;
		break;
	case VK_SHADER_STAGE_FRAGMENT_BIT:
		shader_kind = shaderc_fragment_shader;
		break;
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		shader_kind = shaderc_tess_control_shader;
		break;
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		shader_kind = shaderc_tess_evaluation_shader;
		break;
	case VK_SHADER_STAGE_GEOMETRY_BIT:
		shader_kind = shaderc_geometry_shader;
		break;
	case VK_SHADER_STAGE_COMPUTE_BIT:
		shader_kind = shaderc_compute_shader;
		break;
	case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
		shader_kind = shaderc_raygen_shader;
		break;
	case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		shader_kind = shaderc_anyhit_shader;
		break;
	case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		shader_kind = shaderc_closesthit_shader;
		break;
	case VK_SHADER_STAGE_MISS_BIT_KHR:
		shader_kind = shaderc_miss_shader;
		break;
	case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		shader_kind = shaderc_intersection_shader;
		break;
	case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
		shader_kind = shaderc_callable_shader;
		break;
	case VK_SHADER_STAGE_TASK_BIT_EXT:
		shader_kind = shaderc_task_shader;
		break;
	case VK_SHADER_STAGE_MESH_BIT_EXT:
		shader_kind = shaderc_mesh_shader;
		break;
	default:
		break;
	}

	//compiling our shader into byte code
	shaderc_compilation_result_t compiled_shader = shaderc_compile_into_spv(shader_compiler, shaderSource, strlen(shaderSource), shader_kind, shader_name, entryPoint, compile_options);


	// make compiled shader, vulkan shaders
	if (shaderc_result_get_compilation_status(compiled_shader) == shaderc_compilation_status_success)
	{
		//the Vulkan Shader 
		VkShaderModuleCreateInfo shaderinfo{};
		shaderinfo.sType	= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderinfo.codeSize = shaderc_result_get_length(compiled_shader);
		shaderinfo.pCode	= (uint32_t*)shaderc_result_get_bytes(compiled_shader);

		VkResult result = VK_SUCCESS;
		VK_CALL_PRINT(vkCreateShaderModule(GAPI.VulkanDevice, &shaderinfo, nullptr, &shader))
	}
	else
	{
		printf("Shader compilation failed:\n%s", shaderc_result_get_error_message(compiled_shader));
		return false;
	}


	shaderc_result_release(compiled_shader);

	//release both compile options and compiler
	shaderc_compile_options_release(compile_options);
	shaderc_compiler_release(shader_compiler);

	return true;
}

/*===== BUFFERS =====*/

bool CreateVulkanBuffer(GraphicsAPIManager& GAPI, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkResult result = VK_SUCCESS;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CALL_PRINT(vkCreateBuffer(GAPI.VulkanDevice, &bufferInfo, nullptr, &buffer))
	
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(GAPI.VulkanDevice, buffer, &memRequirements);
	
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(GAPI.VulkanGPU, &memProperties);
	
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if (memRequirements.memoryTypeBits & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			allocInfo.memoryTypeIndex = i;
			break;
		}
	}

	VK_CALL_PRINT(vkAllocateMemory(GAPI.VulkanDevice, &allocInfo, nullptr, &bufferMemory))

	VK_CALL_PRINT(vkBindBufferMemory(GAPI.VulkanDevice, buffer, bufferMemory, 0));

	return result == VK_SUCCESS;
}

bool CreateUniformBufferHandle(GraphicsAPIManager& GAPI, UniformBufferHandle& bufferHandle, uint32_t bufferNb, VkDeviceSize size,
	VkMemoryPropertyFlags properties, VkBufferUsageFlags usage, bool mapCPUMemoryHandle)

{
	VkResult result = VK_SUCCESS;

	//clear if not already empty
	ClearUniformBufferHandle(GAPI, bufferHandle);

	//alloc for buffers
	bufferHandle.GPUBuffer.Alloc(bufferNb);
	bufferHandle.GPUMemoryHandle.Alloc(bufferNb);
	bufferHandle.CPUMemoryHandle.Alloc(bufferNb);

	for (uint32_t i = 0; i < bufferNb; i++)
	{
		//create the new buffer and associated gpu memory with parameter
		result = CreateVulkanBuffer(GAPI, size, usage, properties, bufferHandle.GPUBuffer[i], bufferHandle.GPUMemoryHandle[i]) ? VK_SUCCESS : VK_ERROR_UNKNOWN;

		//link the GPU handlke with the cpu handle if necessary
		if (mapCPUMemoryHandle && result == VK_SUCCESS)
			VK_CALL_PRINT(vkMapMemory(GAPI.VulkanDevice, bufferHandle.GPUMemoryHandle[i], 0, size, 0, &bufferHandle.CPUMemoryHandle[i]));
	}

	return result == VK_SUCCESS;
}

void ClearUniformBufferHandle(GraphicsAPIManager& GAPI, UniformBufferHandle& bufferHandle)
{
	VK_CLEAR_ARRAY(bufferHandle.GPUBuffer, GAPI.NbVulkanFrames, vkDestroyBuffer, GAPI.VulkanDevice);
	VK_CLEAR_ARRAY(bufferHandle.GPUMemoryHandle, GAPI.NbVulkanFrames, vkFreeMemory, GAPI.VulkanDevice);
	bufferHandle.CPUMemoryHandle.Clear();
}


bool CreateStaticBufferHandle(GraphicsAPIManager& GAPI, StaticBufferHandle& bufferHandle, VkDeviceSize size, VkBufferUsageFlags staticBufferUsage, VkMemoryPropertyFlags staticBufferProperties)
{
	//creating the static buffer
	bool toReturn = CreateVulkanBuffer(GAPI, size, staticBufferUsage, staticBufferProperties, bufferHandle.StaticGPUBuffer, bufferHandle.StaticGPUMemoryHandle);

	//creating the staging buffer. properties make it easy to send data from GPU to.
	return toReturn && CreateVulkanBuffer(GAPI, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bufferHandle.StagingGPUBuffer, bufferHandle.StagingGPUMemoryHandle);
}

/* sends the data through the staging buffer to the static buffer */
bool UploadStaticBufferHandle(GraphicsAPIManager& GAPI, StaticBufferHandle& bufferHandle, void* data, VkDeviceSize size, bool releaseStagingBuffer)
{
	
	VkResult result = VK_SUCCESS;

	void* CPUHandle;

	VK_CALL_PRINT(vkMapMemory(GAPI.VulkanDevice, bufferHandle.StagingGPUMemoryHandle, 0, size, 0, &CPUHandle));
	memcpy(CPUHandle, data, size);
	vkUnmapMemory(GAPI.VulkanDevice, bufferHandle.StagingGPUMemoryHandle);


	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level					= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool			= GAPI.VulkanCommandPool[0];
	allocInfo.commandBufferCount	= 1;

	VkCommandBuffer commandBuffer;
	VK_CALL_PRINT(vkAllocateCommandBuffers(GAPI.VulkanDevice, &allocInfo, &commandBuffer));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CALL_PRINT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, bufferHandle.StagingGPUBuffer, bufferHandle.StaticGPUBuffer, 1, &copyRegion);

	VK_CALL_PRINT(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo{};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers		= &commandBuffer;

	VK_CALL_PRINT(vkQueueSubmit(GAPI.RuntimeHandle.VulkanQueues[0], 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL_PRINT(vkQueueWaitIdle(GAPI.RuntimeHandle.VulkanQueues[0]));

	vkFreeCommandBuffers(GAPI.VulkanDevice, GAPI.VulkanCommandPool[0], 1, &commandBuffer);

	if (releaseStagingBuffer)
	{
		vkDestroyBuffer(GAPI.VulkanDevice, bufferHandle.StagingGPUBuffer, nullptr);
		bufferHandle.StagingGPUBuffer = nullptr;
		vkFreeMemory(GAPI.VulkanDevice, bufferHandle.StagingGPUMemoryHandle, nullptr);
		bufferHandle.StagingGPUMemoryHandle = nullptr;
	}

	return result == VK_SUCCESS;
}

void ClearStaticBufferHandle(GraphicsAPIManager& GAPI, StaticBufferHandle& bufferHandle)
{
	if (bufferHandle.StagingGPUBuffer != nullptr)
		vkDestroyBuffer(GAPI.VulkanDevice, bufferHandle.StagingGPUBuffer, nullptr);
	bufferHandle.StagingGPUBuffer = nullptr;
	if (bufferHandle.StagingGPUMemoryHandle != nullptr)
		vkFreeMemory(GAPI.VulkanDevice, bufferHandle.StagingGPUMemoryHandle, nullptr);
	bufferHandle.StagingGPUMemoryHandle = nullptr;
	if (bufferHandle.StaticGPUBuffer != nullptr)
		vkDestroyBuffer(GAPI.VulkanDevice, bufferHandle.StaticGPUBuffer, nullptr);
	bufferHandle.StaticGPUBuffer = nullptr;
	if (bufferHandle.StaticGPUMemoryHandle != nullptr)
		vkFreeMemory(GAPI.VulkanDevice, bufferHandle.StaticGPUMemoryHandle, nullptr);
	bufferHandle.StaticGPUMemoryHandle = nullptr;
}


void LoadObjFile(GraphicsAPIManager& GAPI, const char* fileName, StaticBufferHandle& vertexBuffer, NumberedArray<StaticBufferHandle>& indexBuffer)
{
	// setup obj reader
	tinyobj::ObjReader reader{};
	tinyobj::ObjReaderConfig config{};
	config.triangulate = true;// we ask for triangulate as it will be easier to process

	//read obj
	reader.ParseFromFile(fileName, config);

	//get back data
	const tinyobj::attrib_t& objAttrib = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& objShape = reader.GetShapes();


	// 1. creating and uploading the vertex buffer
	CreateStaticBufferHandle(GAPI, vertexBuffer, sizeof(tinyobj::real_t)*objAttrib.vertices.size());
	UploadStaticBufferHandle(GAPI, vertexBuffer, (void*)objAttrib.vertices.data(), sizeof(tinyobj::real_t) * objAttrib.vertices.size());


	//allocate buffer
	indexBuffer.Alloc(objShape.size());

	for (int32_t i = 0; i < objShape.size(); i++)
	{
		//as ee triangulated, teh number of vertices
		uint32_t indexNb = objShape[i].mesh.indices.size();
		SimpleArray<uint32_t> indices{ indexNb };
		for (int32_t j = 0; j < indexNb; j++)
			indices[j] = objShape[i].mesh.indices[j].vertex_index;

		CreateStaticBufferHandle(GAPI, indexBuffer[i], indexNb * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		UploadStaticBufferHandle(GAPI, indexBuffer[i], (void*)*indices, indexNb * sizeof(uint32_t));
	}
}
