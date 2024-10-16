#include "VulkanHelper.h"

#include "GraphicsAPIManager.h"

//vulkan shader compiler
#include "shaderc/shaderc.h"

#include "Maths.h"

//loader include
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


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

	// first copy the data in the CPU memory allocated by Vulkan for transfer
	VK_CALL_PRINT(vkMapMemory(GAPI.VulkanDevice, bufferHandle.StagingGPUMemoryHandle, 0, size, 0, &CPUHandle));
	memcpy(CPUHandle, data, size);
	vkUnmapMemory(GAPI.VulkanDevice, bufferHandle.StagingGPUMemoryHandle);

	//then create a copy-only command buffer 
	//(in this case we use the graphics command pool but we could use a copy and transfer only command pool later on)
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level					= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool			= GAPI.VulkanCommandPool[0];
	allocInfo.commandBufferCount	= 1;

	VkCommandBuffer commandBuffer;
	VK_CALL_PRINT(vkAllocateCommandBuffers(GAPI.VulkanDevice, &allocInfo, &commandBuffer));

	// beginning the copy command
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CALL_PRINT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	// copying from staging buffer to static buffer
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, bufferHandle.StagingGPUBuffer, bufferHandle.StaticGPUBuffer, 1, &copyRegion);

	VK_CALL_PRINT(vkEndCommandBuffer(commandBuffer));

	// then submit copy command immediately 
	// (preferably, I would want make a copy command for all init copy but for now, it works)
	VkSubmitInfo submitInfo{};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers		= &commandBuffer;

	VK_CALL_PRINT(vkQueueSubmit(GAPI.RuntimeHandle.VulkanQueues[0], 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL_PRINT(vkQueueWaitIdle(GAPI.RuntimeHandle.VulkanQueues[0]));

	vkFreeCommandBuffers(GAPI.VulkanDevice, GAPI.VulkanCommandPool[0], 1, &commandBuffer);

	// free allocated memory and buffer if user wants to
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

void LoadObjFile(GraphicsAPIManager& GAPI, const char* fileName, LoopArray<Mesh>& meshes)
{
	// setup obj reader
	tinyobj::ObjReader reader{};
	tinyobj::ObjReaderConfig config{};
	config.triangulate = true;// we ask for triangulate as it will be easier to process

	//read obj
	if (!reader.ParseFromFile(fileName, config))
		return;

	//get back data
	const tinyobj::attrib_t& objAttrib = reader.GetAttrib();
	const std::vector<tinyobj::shape_t>& objShape = reader.GetShapes();

	//allocate for mesh
	meshes.Alloc(objShape.size());

	bool hasNormals = objAttrib.normals.size() > 0;
	bool hasTexCoords = objAttrib.texcoords.size() > 0;

#if 0
	uint32_t totalIndexNb = 0;
	for (uint32_t i = 0; i < objShape.size(); i++)
		totalIndexNb += objShape[i].mesh.indices.size();
	HeapMemory<uint32_t> indices{ totalIndexNb };

	//allocate for the vertices buffers (there may be more uvs or normals in the attribute, but it is always linked to a single vertex anyway)
	HeapMemory<float> pos{ totalIndexNb * 3u};
	HeapMemory<float> tex_coords{totalIndexNb *2u };
	HeapMemory<float> normals{ totalIndexNb * 3u };

	uint32_t indexOffset = 0;
	for (uint32_t i = 0; i < objShape.size(); i++)
	{
		uint32_t indexNb = objShape[i].mesh.indices.size();

		//make the indices, texcoords and normal
		for (uint32_t j = 0; j < indexNb; j++)
		{
			const tinyobj::index_t& j_index = objShape[i].mesh.indices[j];

			indices[j + indexOffset] = j + indexOffset;

			pos[3 * (j + indexOffset)] = objAttrib.vertices[3 * j_index.vertex_index];
			pos[3 * (j + indexOffset) + 1] = objAttrib.vertices[3 * j_index.vertex_index + 1];
			pos[3 * (j + indexOffset) + 2] = objAttrib.vertices[3 * j_index.vertex_index + 2];

			if (hasTexCoords && j_index.texcoord_index > 0)
			{
				tex_coords[2 * (j + indexOffset)] = objAttrib.texcoords[2 * j_index.texcoord_index];
				tex_coords[2 * (j + indexOffset) + 1] = objAttrib.texcoords[2 * j_index.texcoord_index + 1];
			}

			if (hasNormals && j_index.normal_index > 0)
			{
				normals[3 * (j + indexOffset)] = objAttrib.normals[3 * j_index.normal_index];
				normals[3 * (j + indexOffset) + 1] = objAttrib.normals[3 * j_index.normal_index + 1];
				normals[3 * (j + indexOffset) + 2] = objAttrib.normals[3 * j_index.normal_index + 2];
			}
		}

		// set the number of indices to allow drawing of mesh
		meshes[i].indicesNb = indexNb;
		//set the offest in the indices buffer to draw the mesh
		meshes[i].indicesOffset = indexOffset;

		indexOffset += indexNb;
	}

	// 1. creating and uploading the position buffer
	CreateStaticBufferHandle(GAPI, meshes[0].positions, sizeof(vec3) * totalIndexNb);
	UploadStaticBufferHandle(GAPI, meshes[0].positions, (void*)*pos, sizeof(vec3) * totalIndexNb);

	// 2. creating and uploading the indices buffer (same index buffer is used for all of the meshes, but with different offset)
	CreateStaticBufferHandle(GAPI, meshes[0].indices, sizeof(uint32_t) * totalIndexNb, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	UploadStaticBufferHandle(GAPI, meshes[0].indices, (void*)*indices, sizeof(uint32_t) * totalIndexNb);

	// 3. creating and uploading the uvs buffer
	if (hasTexCoords)
	{
		CreateStaticBufferHandle(GAPI, meshes[0].uvs, sizeof(vec2) * totalIndexNb);
		UploadStaticBufferHandle(GAPI, meshes[0].uvs, (void*)*tex_coords, sizeof(vec2) * totalIndexNb);
	}

	// 4. creating and uploading the normal buffer
	if (hasNormals)
	{
		CreateStaticBufferHandle(GAPI, meshes[0].normals, sizeof(vec3) * totalIndexNb);
		UploadStaticBufferHandle(GAPI, meshes[0].normals, (void*)*normals, sizeof(vec3) * totalIndexNb);
	}

	//copy the unique vertex buffers inside all meshes, to use same buffers for all
	for (uint32_t i = 1; i < meshes.Nb(); i++)
	{
		meshes[i].positions = meshes[0].positions;
		meshes[i].indices	= meshes[0].indices;
		meshes[i].uvs		= meshes[0].uvs;
		meshes[i].normals	= meshes[0].normals;
		meshes[i].tangents	= meshes[0].tangents;
	}
#else
	//get the size of the array
	uint32_t nbVertices = objAttrib.vertices.size() / 3;

	//allocate for the vertices buffers (there may be more uvs or normals in the attribute, but it is always linked to a single vertex anyway)
	HeapMemory<float> tex_coords{ nbVertices * 2u };
	HeapMemory<float> normals{ nbVertices * 3u };

	uint32_t totalIndexNb = 0;
	for (uint32_t i = 0; i < objShape.size(); i++)
		totalIndexNb += objShape[i].mesh.indices.size();
	HeapMemory<uint32_t> indices{ totalIndexNb };

	uint32_t indexOffset = 0;
	for (uint32_t i = 0; i < objShape.size(); i++)
	{
		uint32_t indexNb = objShape[i].mesh.indices.size();

		//make the indices, texcoords and normal
		for (uint32_t j = 0; j < indexNb; j++)
		{
			const tinyobj::index_t& j_index = objShape[i].mesh.indices[j];

			indices[j + indexOffset] = j_index.vertex_index;

			if (hasTexCoords && j_index.texcoord_index > 0)
			{
				tex_coords[2 * j_index.vertex_index] = objAttrib.texcoords[2 * j_index.texcoord_index];
				tex_coords[2 * j_index.vertex_index + 1] = objAttrib.texcoords[2 * j_index.texcoord_index + 1];
			}

			if (hasNormals && j_index.normal_index > 0)
			{
				normals[3 * j_index.vertex_index]	= objAttrib.normals[3 * j_index.normal_index];
				normals[3 * j_index.vertex_index + 1] = objAttrib.normals[3 * j_index.normal_index + 1];
				normals[3 * j_index.vertex_index + 2] = objAttrib.normals[3 * j_index.normal_index + 2];
			}
		}

		// set the number of indices to allow drawing of mesh
		meshes[i].indicesNb = indexNb;
		//set the offest in the indices buffer to draw the mesh
		meshes[i].indicesOffset = indexOffset;

		indexOffset += indexNb;
	}

	// 1. creating and uploading the position buffer
	CreateStaticBufferHandle(GAPI, meshes[0].positions, sizeof(vec3) * nbVertices);
	UploadStaticBufferHandle(GAPI, meshes[0].positions, (void*)objAttrib.vertices.data(), sizeof(vec3) * nbVertices);

	// 2. creating and uploading the indices buffer (same index buffer is used for all of the meshes, but with different offset)
	CreateStaticBufferHandle(GAPI, meshes[0].indices, sizeof(uint32_t) * totalIndexNb, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	UploadStaticBufferHandle(GAPI, meshes[0].indices, (void*)*indices, sizeof(uint32_t) * totalIndexNb);

	// 3. creating and uploading the uvs buffer
	if (hasTexCoords)
	{
		CreateStaticBufferHandle(GAPI, meshes[0].uvs, sizeof(vec2) * nbVertices);
		UploadStaticBufferHandle(GAPI, meshes[0].uvs, (void*)*tex_coords, sizeof(vec2) * nbVertices);
	}

	// 4. creating and uploading the normal buffer
	if (hasNormals)
	{
		CreateStaticBufferHandle(GAPI, meshes[0].normals, sizeof(vec3) * nbVertices);
		UploadStaticBufferHandle(GAPI, meshes[0].normals, (void*)*normals, sizeof(vec3) * nbVertices);
	}

	//copy the unique vertex buffers inside all meshes, to use same buffers for all
	for (uint32_t i = 1; i < meshes.Nb(); i++)
	{
		meshes[i].positions = meshes[0].positions;
		meshes[i].indices	= meshes[0].indices;
		meshes[i].uvs		= meshes[0].uvs;
		meshes[i].normals	= meshes[0].normals;
		meshes[i].tangents	= meshes[0].tangents;
	}
#endif

}

void ClearMesh(GraphicsAPIManager& GAPI, Mesh& mesh)
{
	ClearStaticBufferHandle(GAPI, mesh.indices);
	ClearStaticBufferHandle(GAPI, mesh.positions);
	ClearStaticBufferHandle(GAPI, mesh.normals);
	ClearStaticBufferHandle(GAPI, mesh.uvs);
	ClearStaticBufferHandle(GAPI, mesh.tangents);
}

/* Images/Textures */
bool CreateImage(GraphicsAPIManager& GAPI, VkImage& imageToMake, VkDeviceMemory& imageMemory, uint32_t width, uint32_t height, uint32_t depth, VkImageType imagetype, VkFormat format, VkImageUsageFlags usageFlags, VkMemoryPropertyFlagBits memoryProperties)
{
	//will stay the same if we suceed
	VkResult result = VK_SUCCESS;

	// creating the image object with data given in parameters
	VkImageCreateInfo imageInfo{};
	imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType		= imagetype;//can be 2D or 3D image (even if it is quite unexpected)
	imageInfo.format		= format;//that depends on the number of channels
	imageInfo.extent.width	= width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth	= depth;//kept it in case we have 3D textures
	imageInfo.mipLevels		= 1u;
	imageInfo.arrayLayers	= 1u;
	imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL;//will only load images from buffers, so no need to change this
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//same this can be changed later and each scene will do what they need on their own
	imageInfo.usage			= usageFlags;//this is slighlty more important as we may use image as deffered rendering buffers, so made it available
	imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;//we have to command queues : one for the app, one for the UI. the UI shouldn't use iamge we create.
	imageInfo.samples		= VK_SAMPLE_COUNT_1_BIT;//why would you want more than one sample in a simple app ?
	imageInfo.flags			= 0; // Optional

	VK_CALL_PRINT(vkCreateImage(GAPI.VulkanDevice, &imageInfo, nullptr, &imageToMake));

	//getting the necessary requirements to create our image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(GAPI.VulkanDevice, imageToMake, &memRequirements);

	//getting our device's limitation
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(GAPI.VulkanGPU, &memProperties);

	//trying to find a matching memory type between what the app wants and the device's limitation.
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if (memRequirements.memoryTypeBits & (1 << i)
			&& (memProperties.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties)
		{
			allocInfo.memoryTypeIndex = i;
			break;
		}
	}

	//allocating and associate the memory to our image.
	VK_CALL_PRINT(vkAllocateMemory(GAPI.VulkanDevice, &allocInfo, nullptr, &imageMemory));
	VK_CALL_PRINT(vkBindImageMemory(GAPI.VulkanDevice, imageToMake, imageMemory, 0))

	return result == VK_SUCCESS;
}

bool UploadImage(GraphicsAPIManager& GAPI, VkImage& imageToUploadTo, void* imageContent, uint32_t width, uint32_t height, uint32_t channels, uint32_t depth)
{
	VkResult result = VK_SUCCESS;
	uint32_t size = width * height * depth * channels;
	void* CPUHandle;

	// create a staging buffer taht can hold the images data
	VkBuffer tempBuffer;
	VkDeviceMemory tempMemory;
	CreateVulkanBuffer(GAPI, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tempBuffer, tempMemory);

	// first copy the data in the CPU memory allocated by Vulkan for transfer
	VK_CALL_PRINT(vkMapMemory(GAPI.VulkanDevice, tempMemory, 0, size, 0, &CPUHandle));
	memcpy(CPUHandle, imageContent, size);
	vkUnmapMemory(GAPI.VulkanDevice, tempMemory);

	//then create a copy-only command buffer 
	//(in this case we use the graphics command pool but we could use a copy and transfer only command pool later on)
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = GAPI.VulkanCommandPool[0];
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	VK_CALL_PRINT(vkAllocateCommandBuffers(GAPI.VulkanDevice, &allocInfo, &commandBuffer));

	// beginning the copy command
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CALL_PRINT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	// changing the image to allow transfer from buffer
	VkImageMemoryBarrier barrier{};
	barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout						= VK_IMAGE_LAYOUT_UNDEFINED;//from nothing
	barrier.newLayout						= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;//to transfer dest
	barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.image							= imageToUploadTo;
	barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel	= 0;
	barrier.subresourceRange.levelCount		= 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount		= 1;
	barrier.srcAccessMask					= 0;// making the image accessible to
	barrier.dstAccessMask					= VK_ACCESS_TRANSFER_WRITE_BIT;// ... resources during TRANSFER WRITE

	//layout should change when we go from pipeline doing nothing ...
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	// copying from staging buffer to image
	VkBufferImageCopy copyRegion{};
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent.width	= width;
	copyRegion.imageExtent.height	= height;
	copyRegion.imageExtent.depth	= depth;
	vkCmdCopyBufferToImage(commandBuffer, tempBuffer, imageToUploadTo, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	VK_CALL_PRINT(vkEndCommandBuffer(commandBuffer));

	// then submit copy command immediately 
	// (preferably, I would want make a copy command for all init copy but for now, it works)
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CALL_PRINT(vkQueueSubmit(GAPI.RuntimeHandle.VulkanQueues[0], 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL_PRINT(vkQueueWaitIdle(GAPI.RuntimeHandle.VulkanQueues[0]));

	vkFreeCommandBuffers(GAPI.VulkanDevice, GAPI.VulkanCommandPool[0], 1, &commandBuffer);

	// free allocated memory and buffer
	vkDestroyBuffer(GAPI.VulkanDevice, tempBuffer, nullptr);
	vkFreeMemory(GAPI.VulkanDevice, tempMemory, nullptr);


	return result == VK_SUCCESS;
}

bool LoadTexture(GraphicsAPIManager& GAPI, const char* fileName, VkImage& imageToMake, VkDeviceMemory& imageMemory)
{
	VkResult result = VK_SUCCESS;

	// first loading the texture
	int32_t width, height, channels{0};
	stbi_uc* pixels = stbi_load(fileName, &width, &height, &channels, STBI_rgb_alpha);

	if (pixels == nullptr)
		return false;

	// create a staging buffer taht can hold the images data
	VkBuffer tempBuffer;
	VkDeviceMemory tempMemory;
	CreateVulkanBuffer(GAPI, width * height * channels, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tempBuffer, tempMemory);

	VkFormat imageFormat;
	switch (channels)
	{
		case 1:
			imageFormat = VK_FORMAT_R8_SNORM;
		break;
		case 2:
			imageFormat = VK_FORMAT_R8G8_SNORM;
		break;
		case 3:
			imageFormat = VK_FORMAT_R8G8B8_SNORM;
		break;
		case 4:
			imageFormat = VK_FORMAT_R8G8B8A8_SNORM;
		break;
		default:
			imageFormat = VK_FORMAT_UNDEFINED;
		printf("error when loading %s,  with %d channels.\n is the path correct?", fileName, channels);
		stbi_image_free(pixels);
		return false;
	}

	//creating image from stbi info
	if (!CreateImage(GAPI, imageToMake, imageMemory, width, height, 1u, VK_IMAGE_TYPE_2D, imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		stbi_image_free(pixels);
		return false;
	}

	//upload image to buffer
	if (!UploadImage(GAPI, imageToMake, (void*)pixels, width, height, channels))
	{
		stbi_image_free(pixels);
		return false;
	}

	return result == VK_SUCCESS;
}
