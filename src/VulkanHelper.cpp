#include "VulkanHelper.h"

#include "GraphicsAPIManager.h"

//vulkan shader compiler
#include "shaderc/shaderc.h"

#include "Maths.h"

//loader include
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb_image.h"
#define TINYGLTF_USE_RAPIDJSON 
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

using namespace VulkanHelper;

/*===== Uploader =====*/

bool VulkanHelper::StartUploader(GraphicsAPIManager& GAPI, Uploader& VulkanUploader)
{
	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	//first fill with the data we already have 
	VulkanUploader.VulkanDevice = GAPI.VulkanDevice;
	VulkanUploader.CopyQueue	= GAPI.RuntimeHandle.VulkanQueues[0];
	VulkanUploader.CopyPool		= GAPI.VulkanCommandPool[0];

	//then create a copy-only command buffer 
	//(in this case we use the graphics command pool but we could use a copy and transfer only command pool later on)
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level					= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool			= GAPI.VulkanCommandPool[0];
	allocInfo.commandBufferCount	= 1;

	VK_CALL_PRINT(vkAllocateCommandBuffers(GAPI.VulkanDevice, &allocInfo, &VulkanUploader.CopyBuffer));

	// beginning the copy command
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CALL_PRINT(vkBeginCommandBuffer(VulkanUploader.CopyBuffer, &beginInfo));

	//also get the memory properties to create buffers and images
	vkGetPhysicalDeviceMemoryProperties(GAPI.VulkanGPU, &VulkanUploader.MemoryProperties);

	return result == VK_SUCCESS;
}


bool VulkanHelper::SubmitUploader(Uploader& VulkanUploader)
{
	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	VK_CALL_PRINT(vkEndCommandBuffer(VulkanUploader.CopyBuffer));

	// submit copy command immediately 
	VkSubmitInfo submitInfo{};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers		= &VulkanUploader.CopyBuffer;

	VK_CALL_PRINT(vkQueueSubmit(VulkanUploader.CopyQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL_PRINT(vkQueueWaitIdle(VulkanUploader.CopyQueue));

	VK_CLEAR_LIST(VulkanUploader.ToFreeBuffers, VulkanUploader.ToFreeBuffers.GetNb(), vkDestroyBuffer, VulkanUploader.VulkanDevice);
	VK_CLEAR_LIST(VulkanUploader.ToFreeMemory, VulkanUploader.ToFreeMemory.GetNb(), vkFreeMemory, VulkanUploader.VulkanDevice);

	vkFreeCommandBuffers(VulkanUploader.VulkanDevice, VulkanUploader.CopyPool, 1, &VulkanUploader.CopyBuffer);

	return result == VK_SUCCESS;
}

/*===== SHADERS =====*/

bool VulkanHelper::CreateVulkanShaders(Uploader& VulkanUploader, VkShaderModule& shader, VkShaderStageFlagBits shaderStage, const char* shaderSource, const char* shader_name, const char* entryPoint)
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
		VK_CALL_PRINT(vkCreateShaderModule(VulkanUploader.VulkanDevice, &shaderinfo, nullptr, &shader))
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

uint32_t VulkanHelper::GetMemoryTypeFromRequirements(const VkMemoryPropertyFlags& wantedMemoryProperties, const VkMemoryRequirements& memoryRequirements, const VkPhysicalDeviceMemoryProperties& memoryProperties)
{
	//basically, looping through all memory properties this device is capable of (GPU), 
	//until we find something that satisfies the properties that we watn and that the buffer needs
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
		if (memoryRequirements.memoryTypeBits & (1 << i) 
			&& (memoryProperties.memoryTypes[i].propertyFlags & wantedMemoryProperties) == wantedMemoryProperties) {
			return i;
		}
	}
	return 0;
}

bool VulkanHelper::CreateVulkanBuffer(Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, uint32_t offset, bool AllocateMemory)
{
	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	//creating a buffer with info given in parameters
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size			= size;
	bufferInfo.usage		= usage;
	bufferInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VK_CALL_PRINT(vkCreateBuffer(VulkanUploader.VulkanDevice, &bufferInfo, nullptr, &buffer))
	
	//now that we have created our buffer object, we need to allocate GPU memory associated with it
	//first let's get the requirements that the memory should follow (like it needs to be 4 bytes and such)
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(VulkanUploader.VulkanDevice, buffer, &memRequirements);
	if (AllocateMemory)
	{


		//now that we have reauirements like size we also should choose the type.
		//we choose the first type that will go with the properties user asked for 
		//(using the flags to get the better performing memory type)
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = GetMemoryTypeFromRequirements(properties, memRequirements, VulkanUploader.MemoryProperties);

		VK_CALL_PRINT(vkAllocateMemory(VulkanUploader.VulkanDevice, &allocInfo, nullptr, &bufferMemory))
	}

	VK_CALL_PRINT(vkBindBufferMemory(VulkanUploader.VulkanDevice, buffer, bufferMemory, offset));

	return result == VK_SUCCESS;
}

bool VulkanHelper::CreateUniformBufferHandle(Uploader& VulkanUploader, UniformBufferHandle& bufferHandle, uint32_t bufferNb, VkDeviceSize size,
	VkMemoryPropertyFlags properties, VkBufferUsageFlags usage, bool mapCPUMemoryHandle)

{
	VkResult result = VK_SUCCESS;

	//clear if not already empty
	ClearUniformBufferHandle(VulkanUploader.VulkanDevice, bufferHandle);

	//alloc for buffers
	bufferHandle.GPUBuffer.Alloc(bufferNb);
	bufferHandle.GPUMemoryHandle.Alloc(bufferNb);
	bufferHandle.CPUMemoryHandle.Alloc(bufferNb);
	bufferHandle.nbBuffer = bufferNb;

	for (uint32_t i = 0; i < bufferNb; i++)
	{
		//create the new buffer and associated gpu memory with parameter
		result = CreateVulkanBuffer(VulkanUploader, size, usage, properties, bufferHandle.GPUBuffer[i], bufferHandle.GPUMemoryHandle[i]) ? VK_SUCCESS : VK_ERROR_UNKNOWN;

		//link the GPU handlke with the cpu handle if necessary
		if (mapCPUMemoryHandle && result == VK_SUCCESS)
			VK_CALL_PRINT(vkMapMemory(VulkanUploader.VulkanDevice, bufferHandle.GPUMemoryHandle[i], 0, size, 0, &bufferHandle.CPUMemoryHandle[i]));
	}

	return result == VK_SUCCESS;
}

void VulkanHelper::ClearUniformBufferHandle(const VkDevice& VulkanDevice, UniformBufferHandle& bufferHandle)
{
	VK_CLEAR_ARRAY(bufferHandle.GPUBuffer, bufferHandle.nbBuffer, vkDestroyBuffer, VulkanDevice);
	VK_CLEAR_ARRAY(bufferHandle.GPUMemoryHandle, bufferHandle.nbBuffer, vkFreeMemory, VulkanDevice);
	bufferHandle.CPUMemoryHandle.Clear();
}


bool VulkanHelper::CreateStaticBufferHandle(Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, VkDeviceSize size, VkBufferUsageFlags staticBufferUsage, VkMemoryPropertyFlags staticBufferProperties)
{
	//creating the static buffer
	return CreateVulkanBuffer(VulkanUploader, size, staticBufferUsage, staticBufferProperties, bufferHandle.StaticGPUBuffer, bufferHandle.StaticGPUMemoryHandle, 0);
}

/* sends the data through the staging buffer to the static buffer */
bool VulkanHelper::UploadStaticBufferHandle(Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, void* data, VkDeviceSize size, bool releaseStagingBuffer)
{	
	VkResult result = VK_SUCCESS;

	//temporary CPU memory that we'll throw the data in
	void* CPUHandle;

	//create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingGPUMemoryHandle;
	CreateVulkanBuffer(VulkanUploader, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingGPUMemoryHandle);

	// first copy the data in the CPU memory allocated by Vulkan for transfer
	VK_CALL_PRINT(vkMapMemory(VulkanUploader.VulkanDevice, stagingGPUMemoryHandle, 0, size, 0, &CPUHandle));
	memcpy(CPUHandle, data, size);
	vkUnmapMemory(VulkanUploader.VulkanDevice, stagingGPUMemoryHandle);

	// copying from staging buffer to static buffer
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(VulkanUploader.CopyBuffer, stagingBuffer, bufferHandle.StaticGPUBuffer, 1, &copyRegion);

	// free allocated memory and buffer if user wants to
	if (releaseStagingBuffer)
	{
		VulkanUploader.ToFreeBuffers.Add(stagingBuffer);
		VulkanUploader.ToFreeMemory.Add(stagingGPUMemoryHandle);
	}

	return result == VK_SUCCESS;
}

void VulkanHelper::ClearStaticBufferHandle(const VkDevice& VulkanDevice, StaticBufferHandle& bufferHandle)
{
	if (bufferHandle.StaticGPUBuffer != nullptr)
		vkDestroyBuffer(VulkanDevice, bufferHandle.StaticGPUBuffer, nullptr);
	bufferHandle.StaticGPUBuffer = nullptr;
	if (bufferHandle.StaticGPUMemoryHandle != nullptr)
		vkFreeMemory(VulkanDevice, bufferHandle.StaticGPUMemoryHandle, nullptr);
	bufferHandle.StaticGPUMemoryHandle = nullptr;
}

void VulkanHelper::LoadObjFile(Uploader& VulkanUploader, const char* fileName, LoopArray<Mesh>& meshes)
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
		meshes[i].indicesType = VK_INDEX_TYPE_UINT32;

		indexOffset += indexNb;
	}


	// 1. creating and uploading the position buffer
	StaticBufferHandle posBuffer;
	CreateStaticBufferHandle(VulkanUploader, posBuffer, sizeof(vec3) * nbVertices);
	UploadStaticBufferHandle(VulkanUploader, posBuffer, (void*)objAttrib.vertices.data(), sizeof(vec3) * nbVertices);

	// 2. creating and uploading the indices buffer (same index buffer is used for all of the meshes, but with different offset)
	StaticBufferHandle indexBuffer;
	CreateStaticBufferHandle(VulkanUploader, indexBuffer, sizeof(uint32_t)* totalIndexNb, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	UploadStaticBufferHandle(VulkanUploader, indexBuffer, (void*)*indices, sizeof(uint32_t) * totalIndexNb);

	// 3. creating and uploading the uvs buffer
	StaticBufferHandle uvBuffer;
	if (hasTexCoords)
	{
		CreateStaticBufferHandle(VulkanUploader, uvBuffer, sizeof(vec2) * nbVertices);
		UploadStaticBufferHandle(VulkanUploader, uvBuffer, (void*)*tex_coords, sizeof(vec2) * nbVertices);
	}

	// 4. creating and uploading the normal buffer
	StaticBufferHandle normalBuffer;
	if (hasNormals)
	{
		CreateStaticBufferHandle(VulkanUploader, normalBuffer, sizeof(vec3) * nbVertices);
		UploadStaticBufferHandle(VulkanUploader, normalBuffer, (void*)*normals, sizeof(vec3) * nbVertices);
	}

	//copy the unique vertex buffers inside all meshes, to use same buffers for all
	for (uint32_t i = 0; i < meshes.Nb(); i++)
	{
		meshes[i].vertexMemoryHandle.Alloc(4);
		meshes[i].positions				= posBuffer.StaticGPUBuffer;
		meshes[i].vertexMemoryHandle[0] = posBuffer.StaticGPUMemoryHandle;
		meshes[i].indices				= indexBuffer.StaticGPUBuffer;
		meshes[i].vertexMemoryHandle[1] = indexBuffer.StaticGPUMemoryHandle;
		meshes[i].uvs					= uvBuffer.StaticGPUBuffer;
		meshes[i].vertexMemoryHandle[2] = uvBuffer.StaticGPUMemoryHandle;
		meshes[i].normals				= normalBuffer.StaticGPUBuffer;
		meshes[i].vertexMemoryHandle[3] = normalBuffer.StaticGPUMemoryHandle;
		//meshes[i].tangents				= ;
		//meshes[i].vertexMemoryHandle[4]
	}
#endif

}

void VulkanHelper::ClearMesh(const VkDevice& VulkanDevice, Mesh& mesh)
{
	VK_CLEAR_ARRAY(mesh.vertexMemoryHandle, mesh.vertexMemoryHandle.Nb(), vkFreeMemory, VulkanDevice);
	VK_CLEAR_RAW_ARRAY(mesh.vertexBuffers, 5, vkDestroyBuffer, VulkanDevice);
}

bool VulkanHelper::LoadGLTFFile(Uploader& VulkanUploader, const char* fileName, Model& model)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model loadedModel;
	std::string err;
	std::string warnings;

	//first load model
	if (!loader.LoadASCIIFromFile(&loadedModel, &err, &warnings, fileName))
	{
		printf("error when reading %s :\nerror : %s\nwarning : %s", fileName, err.c_str(), warnings.c_str());
		return false;
	}

	//1. loading every buffer into device memory
	model.buffersHandle.Alloc(loadedModel.buffers.size());
	for (uint32_t i = 0; i < loadedModel.buffers.size(); i++)
	{
		tinygltf::Buffer& buffer = loadedModel.buffers[i];

		//we're using a static because it is easier to upload with, but we actually only want to allocate and send data to device memory
		StaticBufferHandle tmpBufferHandle;
		CreateVulkanBuffer(VulkanUploader, buffer.data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tmpBufferHandle.StaticGPUBuffer, tmpBufferHandle.StaticGPUMemoryHandle, 0);
		UploadStaticBufferHandle(VulkanUploader, tmpBufferHandle, buffer.data.data(), buffer.data.size());

		model.buffersHandle[i] = tmpBufferHandle.StaticGPUMemoryHandle;

		vkDestroyBuffer(VulkanUploader.VulkanDevice, tmpBufferHandle.StaticGPUBuffer, nullptr);
	}

	//2. load every image in memory
	model.textures.Alloc(loadedModel.images.size());
	for (uint32_t i = 0; i < loadedModel.images.size(); i++)
	{
		tinygltf::Image& image = loadedModel.images[i];

		VkFormat imageFormat;
		switch (image.component)
		{
		case 1:
			imageFormat = VK_FORMAT_R8_SRGB;
			break;
		case 2:
			imageFormat = VK_FORMAT_R8G8_SRGB;
			break;
		case 3:
			imageFormat = VK_FORMAT_R8G8B8_SRGB;
			break;
		case 4:
			imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		default:
			imageFormat = VK_FORMAT_UNDEFINED;
			printf("error when loading %s, with %d channels.\n is the path correct?", image.uri, image.component);
			return false;
		}

		LoadTexture(VulkanUploader, (void*)image.image.data(), (uint32_t)image.width, (uint32_t)image.height, imageFormat, model.textures[i]);
	}

	uint32_t totalMeshNb = 0;
	for (uint32_t i = 0; i < loadedModel.meshes.size(); i++)
		totalMeshNb += loadedModel.meshes[i].primitives.size();
	model.meshes.Alloc(totalMeshNb);

	//2. creating vulkan buffer interface for meshes
	uint32_t meshIndex = 0;
	for (uint32_t i = 0; i < loadedModel.scenes[loadedModel.defaultScene].nodes.size(); i++)
	{
		uint32_t nodeIndex = loadedModel.scenes[loadedModel.defaultScene].nodes[i];
		tinygltf::Mesh& indexMesh = loadedModel.meshes[nodeIndex];

		for (uint32_t j = 0; j < indexMesh.primitives.size(); j++)
		{
			tinygltf::Primitive& jPrimitive = indexMesh.primitives[j];

			//create indices buffer from preallocated device memory
			{
				tinygltf::Accessor&	accessor		= loadedModel.accessors[jPrimitive.indices];
				tinygltf::BufferView& bufferView	= loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, (bufferView.byteLength - accessor.byteOffset), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model.meshes[meshIndex].indices, model.buffersHandle[bufferView.buffer], (bufferView.byteOffset + accessor.byteOffset), false);

				model.meshes[meshIndex].indicesNb = accessor.count;
				model.meshes[meshIndex].indicesOffset = 0;

				switch (accessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					model.meshes[meshIndex].indicesType = VK_INDEX_TYPE_UINT8_KHR;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					model.meshes[meshIndex].indicesType = VK_INDEX_TYPE_UINT16;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					model.meshes[meshIndex].indicesType = VK_INDEX_TYPE_UINT32;
					break;
				}
			}

			//position
			if (jPrimitive.attributes.count("POSITION") > 0)
			{
				tinygltf::Accessor& accessor		= loadedModel.accessors[jPrimitive.attributes["POSITION"]];
				tinygltf::BufferView& bufferView	= loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model.meshes[meshIndex].positions, model.buffersHandle[bufferView.buffer], 0, false);

				model.meshes[meshIndex].posOffset = accessor.byteOffset + bufferView.byteOffset;
			}

			//uvs
			if (jPrimitive.attributes.count("TEXCOORD_0") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["TEXCOORD_0"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model.meshes[meshIndex].uvs, model.buffersHandle[bufferView.buffer], 0, false);

				model.meshes[meshIndex].uvOffset = bufferView.byteOffset + accessor.byteOffset;
			}

			//normals
			if (jPrimitive.attributes.count("NORMAL") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["NORMAL"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model.meshes[meshIndex].normals, model.buffersHandle[bufferView.buffer], 0, false);

				model.meshes[meshIndex].normalOffset = bufferView.byteOffset + accessor.byteOffset;
			}

			//tangents
			if (jPrimitive.attributes.count("TANGENT") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["TANGENT"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model.meshes[meshIndex].tangents, model.buffersHandle[bufferView.buffer], 0, false);

				model.meshes[meshIndex].tangentOffset = bufferView.byteOffset + accessor.byteOffset;
			}

			meshIndex++;
		}
	}

	//Models.meshes.Alloc(model.meshes.size());
	//
	//
	//for (uint32_t i = 0; i < model.meshes.size(); i++)
	//{
	//	for (uint32_t j = 0; j < model.meshes[i].primitives.size(); j++)
	//	{
	//
	//		model.meshes[i].primitives[j].indices;
	//		//model.meshes[i].primitives[j].attributes["POSITION"]
	//	}
	//
	//}
	
}

void VulkanHelper::ClearModel(const VkDevice& VulkanDevice, Model& model)
{
	for (uint32_t i = 0; i < model.meshes.Nb(); i++)
	{
		ClearMesh(VulkanDevice, model.meshes[i]);
	}
	VK_CLEAR_ARRAY(model.buffersHandle, model.buffersHandle.Nb(), vkFreeMemory, VulkanDevice);
}

/* Images/Textures */


bool VulkanHelper::CreateImage(Uploader& VulkanUploader, VkImage& imageToMake, VkDeviceMemory& imageMemory, uint32_t width, uint32_t height, uint32_t depth, VkImageType imagetype, VkFormat format, VkImageUsageFlags usageFlags, VkMemoryPropertyFlagBits memoryProperties)
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

	VK_CALL_PRINT(vkCreateImage(VulkanUploader.VulkanDevice, &imageInfo, nullptr, &imageToMake));

	//getting the necessary requirements to create our image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(VulkanUploader.VulkanDevice, imageToMake, &memRequirements);

	//trying to find a matching memory type between what the app wants and the device's limitation.
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = GetMemoryTypeFromRequirements(memoryProperties, memRequirements, VulkanUploader.MemoryProperties);

	//allocating and associate the memory to our image.
	VK_CALL_PRINT(vkAllocateMemory(VulkanUploader.VulkanDevice, &allocInfo, nullptr, &imageMemory));
	VK_CALL_PRINT(vkBindImageMemory(VulkanUploader.VulkanDevice, imageToMake, imageMemory, 0))

	return result == VK_SUCCESS;
}

bool VulkanHelper::UploadImage(Uploader& VulkanUploader, VkImage& imageToUploadTo, void* imageContent, uint32_t width, uint32_t height, uint32_t channels, uint32_t depth)
{
	VkResult result = VK_SUCCESS;
	uint32_t size = width * height * depth * channels;
	void* CPUHandle;

	// create a staging buffer that can hold the images data
	VkBuffer tempBuffer;
	VkDeviceMemory tempMemory;
	CreateVulkanBuffer(VulkanUploader, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tempBuffer, tempMemory);

	// first copy the data in the CPU memory allocated by Vulkan for transfer
	VK_CALL_PRINT(vkMapMemory(VulkanUploader.VulkanDevice, tempMemory, 0, size, 0, &CPUHandle));
	memcpy(CPUHandle, imageContent, size);
	vkUnmapMemory(VulkanUploader.VulkanDevice, tempMemory);

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

	//layout should change when we go from pipeline doing nothing to transfer stage
	vkCmdPipelineBarrier(VulkanUploader.CopyBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	// copying from staging buffer to image
	VkBufferImageCopy copyRegion{};
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent.width	= width;
	copyRegion.imageExtent.height	= height;
	copyRegion.imageExtent.depth	= depth;
	vkCmdCopyBufferToImage(VulkanUploader.CopyBuffer, tempBuffer, imageToUploadTo, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	// free allocated memory and buffer
	VulkanUploader.ToFreeBuffers.Add(tempBuffer);
	VulkanUploader.ToFreeMemory.Add(tempMemory);

	return result == VK_SUCCESS;
}



bool VulkanHelper::LoadTexture(Uploader& VulkanUploader, const char* fileName, Texture& texture)
{
	VkResult result = VK_SUCCESS;

	// first loading the texture
	int32_t width, height, channels{0};
	stbi_uc* pixels = stbi_load(fileName, &width, &height, &channels, STBI_rgb_alpha);

	if (pixels == nullptr)
		return false;

	VkFormat imageFormat;
	switch (channels)
	{
		case 1:
			imageFormat = VK_FORMAT_R8_SRGB;
		break;
		case 2:
			imageFormat = VK_FORMAT_R8G8_SRGB;
		break;
		case 3:
			imageFormat = VK_FORMAT_R8G8B8_SRGB;
		break;
		case 4:
			imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
		break;
		default:
			imageFormat = VK_FORMAT_UNDEFINED;
		printf("error when loading %s,  with %d channels.\n is the path correct?", fileName, channels);
		return false;
	}

	LoadTexture(VulkanUploader, (void*)pixels, width, height, imageFormat, texture);
	stbi_image_free(pixels);

	return result == VK_SUCCESS;
}

#include "vulkan/utility/vk_format_utils.h"

bool VulkanHelper::LoadTexture(Uploader& VulkanUploader, void* pixels, uint32_t width, uint32_t height, VkFormat imageFormat, Texture& texture)
{
	VkResult result = VK_SUCCESS;

	if (pixels == nullptr)
		return false;

	// create a staging buffer taht can hold the images data
	VkBuffer tempBuffer;
	VkDeviceMemory tempMemory;

	CreateVulkanBuffer(VulkanUploader, width * height * vkuFormatElementSize(imageFormat), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tempBuffer, tempMemory);

	//creating image from stbi info
	if (!CreateImage(VulkanUploader, texture.image, texture.imageMemory, width, height, 1u, VK_IMAGE_TYPE_2D, imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		return false;
	}

	//upload image to buffer
	if (!UploadImage(VulkanUploader, texture.image, (void*)pixels, width, height, vkuGetFormatInfo(imageFormat).component_count))
	{
		return false;
	}

	// changing the image to allow read from shader
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;//from nothing
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//to transfer dest
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.image = texture.image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;// making the image accessible for ...
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // shaders

	//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
	vkCmdPipelineBarrier(VulkanUploader.CopyBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	//create an image view associated with the created image to make it available in shader
	VkImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.format = imageFormat;
	viewCreateInfo.image = texture.image;
	viewCreateInfo.subresourceRange.layerCount = 1;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

	VK_CALL_PRINT(vkCreateImageView(VulkanUploader.VulkanDevice, &viewCreateInfo, nullptr, &texture.imageView));

	//create a sampler that can be used to sample this texture inside the shader
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;

	VK_CALL_PRINT(vkCreateSampler(VulkanUploader.VulkanDevice, &samplerCreateInfo, nullptr, &texture.sampler))

	return result == VK_SUCCESS;
}


void VulkanHelper::ClearTexture(const VkDevice& VulkanDevice, Texture& texture)
{
	//first release memory
	if (texture.image != nullptr)
		vkDestroyImage(VulkanDevice, texture.image, nullptr);
	if (texture.imageMemory != nullptr)
		vkFreeMemory(VulkanDevice, texture.imageMemory, nullptr);
	if (texture.imageView != nullptr)
		vkDestroyImageView(VulkanDevice, texture.imageView, nullptr);
	if (texture.sampler != nullptr)
		vkDestroySampler(VulkanDevice, texture.sampler, nullptr);

	//then memset 0
	texture.image		= nullptr;
	texture.imageMemory	= nullptr;
	texture.imageView	= nullptr;
	texture.sampler		= nullptr;
}
