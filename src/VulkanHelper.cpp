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
	VulkanUploader._VulkanDevice = GAPI._VulkanDevice;
	VulkanUploader._CopyQueue	= GAPI._RuntimeHandle._VulkanQueues[0];
	VulkanUploader._CopyPool	= GAPI._VulkanCommandPool[0];

	//then create a copy-only command buffer 
	//(in this case we use the graphics command pool but we could use a copy and transfer only command pool later on)
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level					= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool			= GAPI._VulkanCommandPool[0];
	allocInfo.commandBufferCount	= 1;

	VK_CALL_PRINT(vkAllocateCommandBuffers(GAPI._VulkanDevice, &allocInfo, &VulkanUploader._CopyBuffer));

	// beginning the copy command
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CALL_PRINT(vkBeginCommandBuffer(VulkanUploader._CopyBuffer, &beginInfo));

	//also get the memory properties to create buffers and images
	vkGetPhysicalDeviceMemoryProperties(GAPI._VulkanGPU, &VulkanUploader._MemoryProperties);

	return result == VK_SUCCESS;
}


bool VulkanHelper::SubmitUploader(Uploader& VulkanUploader)
{
	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	VK_CALL_PRINT(vkEndCommandBuffer(VulkanUploader._CopyBuffer));

	// submit copy command immediately 
	VkSubmitInfo submitInfo{};
	submitInfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount	= 1;
	submitInfo.pCommandBuffers		= &VulkanUploader._CopyBuffer;

	VK_CALL_PRINT(vkQueueSubmit(VulkanUploader._CopyQueue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CALL_PRINT(vkQueueWaitIdle(VulkanUploader._CopyQueue));

	VK_CLEAR_LIST(VulkanUploader._ToFreeBuffers, VulkanUploader._ToFreeBuffers.Nb(), vkDestroyBuffer, VulkanUploader._VulkanDevice);
	VK_CLEAR_LIST(VulkanUploader._ToFreeMemory, VulkanUploader._ToFreeMemory.Nb(), vkFreeMemory, VulkanUploader._VulkanDevice);

	vkFreeCommandBuffers(VulkanUploader._VulkanDevice, VulkanUploader._CopyPool, 1, &VulkanUploader._CopyBuffer);

	return result == VK_SUCCESS;
}

/*===== SHADERS =====*/

bool VulkanHelper::CreateVulkanShaders(Uploader& VulkanUploader, VkShaderModule& shader, VkShaderStageFlagBits shaderStage, const char* shader_source, const char* shader_name, const char* entry_point)
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
		shaderc_compile_options_set_target_spirv(compile_options, shaderc_spirv_version_1_4);//raytracing extensions requires at least spirv 1.4
		break;
	case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
		shader_kind = shaderc_anyhit_shader;
		shaderc_compile_options_set_target_spirv(compile_options, shaderc_spirv_version_1_4);//raytracing extensions requires at least spirv 1.4
		break;
	case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		shader_kind = shaderc_closesthit_shader;
		shaderc_compile_options_set_target_spirv(compile_options, shaderc_spirv_version_1_4);//raytracing extensions requires at least spirv 1.4
		break;
	case VK_SHADER_STAGE_MISS_BIT_KHR:
		shader_kind = shaderc_miss_shader;
		shaderc_compile_options_set_target_spirv(compile_options, shaderc_spirv_version_1_4);//raytracing extensions requires at least spirv 1.4
		break;
	case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
		shader_kind = shaderc_intersection_shader;
		shaderc_compile_options_set_target_spirv(compile_options, shaderc_spirv_version_1_4);//raytracing extensions requires at least spirv 1.4
		break;
	case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
		shader_kind = shaderc_callable_shader;
		shaderc_compile_options_set_target_spirv(compile_options, shaderc_spirv_version_1_4);//raytracing extensions requires at least spirv 1.4
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
	shaderc_compilation_result_t compiled_shader = shaderc_compile_into_spv(shader_compiler, shader_source, strlen(shader_source), shader_kind, shader_name, entry_point, compile_options);

	// make compiled shader, vulkan shaders
	if (shaderc_result_get_compilation_status(compiled_shader) == shaderc_compilation_status_success)
	{
		//the Vulkan Shader 
		VkShaderModuleCreateInfo shaderinfo{};
		shaderinfo.sType	= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderinfo.codeSize = shaderc_result_get_length(compiled_shader);
		shaderinfo.pCode	= (uint32_t*)shaderc_result_get_bytes(compiled_shader);

		VkResult result = VK_SUCCESS;
		VK_CALL_PRINT(vkCreateShaderModule(VulkanUploader._VulkanDevice, &shaderinfo, nullptr, &shader))
	}
	else
	{
		uint32_t version, revision;
		shaderc_get_spv_version(&version, &revision);

		printf("Shader compilation failed (SPIRV-V v.%u.%u):\n%s",version, revision, shaderc_result_get_error_message(compiled_shader));
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

bool VulkanHelper::CreateVulkanBuffer(Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, uint32_t offset, bool allocate_memory, VkMemoryAllocateFlags flags)
{
	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	//creating a buffer with info given in parameters
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size			= size;
	bufferInfo.usage		= usage;
	bufferInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VK_CALL_PRINT(vkCreateBuffer(VulkanUploader._VulkanDevice, &bufferInfo, nullptr, &buffer))
	
	//now that we have created our buffer object, we need to allocate GPU memory associated with it
	//first let's get the requirements that the memory should follow (like it needs to be 4 bytes and such)
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(VulkanUploader._VulkanDevice, buffer, &memRequirements);
	if (allocate_memory)
	{
		//now that we have reauirements like size we also should choose the type.
		//we choose the first type that will go with the properties user asked for 
		//(using the flags to get the better performing memory type)
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = GetMemoryTypeFromRequirements(properties, memRequirements, VulkanUploader._MemoryProperties);

		VkMemoryAllocateFlagsInfo flagsInfo{};
		if (flags != 0)
		{
			flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
			flagsInfo.flags = flags;
			allocInfo.pNext = &flagsInfo;
		}

		VK_CALL_PRINT(vkAllocateMemory(VulkanUploader._VulkanDevice, &allocInfo, nullptr, &bufferMemory))
	}

	VK_CALL_PRINT(vkBindBufferMemory(VulkanUploader._VulkanDevice, buffer, bufferMemory, offset));

	return result == VK_SUCCESS;
}

bool VulkanHelper::CreateUniformBufferHandle(Uploader& VulkanUploader, UniformBufferHandle& bufferHandle, uint32_t bufferNb, VkDeviceSize size,
	VkMemoryPropertyFlags properties, VkBufferUsageFlags usage, bool map_cpu_memory_handle)

{
	VkResult result = VK_SUCCESS;

	//clear if not already empty
	ClearUniformBufferHandle(VulkanUploader._VulkanDevice, bufferHandle);

	//alloc for buffers
	bufferHandle._GPUBuffer.Alloc(bufferNb);
	bufferHandle._GPUMemoryHandle.Alloc(bufferNb);
	bufferHandle._CPUMemoryHandle.Alloc(bufferNb);
	bufferHandle._nb_buffer = bufferNb;

	for (uint32_t i = 0; i < bufferNb; i++)
	{
		//create the new buffer and associated gpu memory with parameter
		result = CreateVulkanBuffer(VulkanUploader, size, usage, properties, bufferHandle._GPUBuffer[i], bufferHandle._GPUMemoryHandle[i]) ? VK_SUCCESS : VK_ERROR_UNKNOWN;

		//link the GPU handlke with the cpu handle if necessary
		if (map_cpu_memory_handle && result == VK_SUCCESS)
			VK_CALL_PRINT(vkMapMemory(VulkanUploader._VulkanDevice, bufferHandle._GPUMemoryHandle[i], 0, size, 0, &bufferHandle._CPUMemoryHandle[i]));
	}

	return result == VK_SUCCESS;
}

void VulkanHelper::ClearUniformBufferHandle(const VkDevice& VulkanDevice, UniformBufferHandle& bufferHandle)
{
	VK_CLEAR_ARRAY(bufferHandle._GPUBuffer, bufferHandle._nb_buffer, vkDestroyBuffer, VulkanDevice);
	VK_CLEAR_ARRAY(bufferHandle._GPUMemoryHandle, bufferHandle._nb_buffer, vkFreeMemory, VulkanDevice);
	bufferHandle._CPUMemoryHandle.Clear();
}


bool VulkanHelper::CreateStaticBufferHandle(Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, VkDeviceSize size, VkBufferUsageFlags staticBufferUsage, VkMemoryPropertyFlags staticBufferProperties)
{
	//creating the static buffer
	return CreateVulkanBuffer(VulkanUploader, size, staticBufferUsage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, staticBufferProperties, bufferHandle._StaticGPUBuffer, bufferHandle._StaticGPUMemoryHandle, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
}

/* sends the data through the staging buffer to the static buffer */
bool VulkanHelper::UploadStaticBufferHandle(Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, const void* data, VkDeviceSize size, bool release_staging_buffer)
{	
	VkResult result = VK_SUCCESS;

	//temporary CPU memory that we'll throw the data in
	void* CPUHandle;

	//create staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingGPUMemoryHandle;
	CreateVulkanBuffer(VulkanUploader, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingGPUMemoryHandle);

	// first copy the data in the CPU memory allocated by Vulkan for transfer
	VK_CALL_PRINT(vkMapMemory(VulkanUploader._VulkanDevice, stagingGPUMemoryHandle, 0, size, 0, &CPUHandle));
	memcpy(CPUHandle, data, size);
	vkUnmapMemory(VulkanUploader._VulkanDevice, stagingGPUMemoryHandle);

	// copying from staging buffer to static buffer
	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0; // Optional
	copyRegion.dstOffset = 0; // Optional
	copyRegion.size = size;
	vkCmdCopyBuffer(VulkanUploader._CopyBuffer, stagingBuffer, bufferHandle._StaticGPUBuffer, 1, &copyRegion);

	// free allocated memory and buffer if user wants to
	if (release_staging_buffer)
	{
		VulkanUploader._ToFreeBuffers.Add(stagingBuffer);
		VulkanUploader._ToFreeMemory.Add(stagingGPUMemoryHandle);
	}

	return result == VK_SUCCESS;
}

void VulkanHelper::ClearStaticBufferHandle(const VkDevice& VulkanDevice, StaticBufferHandle& bufferHandle)
{
	if (bufferHandle._StaticGPUBuffer != nullptr)
		vkDestroyBuffer(VulkanDevice, bufferHandle._StaticGPUBuffer, nullptr);
	bufferHandle._StaticGPUBuffer = nullptr;
	if (bufferHandle._StaticGPUMemoryHandle != nullptr)
		vkFreeMemory(VulkanDevice, bufferHandle._StaticGPUMemoryHandle, nullptr);
	bufferHandle._StaticGPUMemoryHandle = nullptr;
}

void VulkanHelper::LoadObjFile(Uploader& VulkanUploader, const char* file_name, VolatileLoopArray<Mesh>& meshes)
{
	// setup obj reader
	tinyobj::ObjReader reader{};
	tinyobj::ObjReaderConfig config{};
	config.triangulate = true;// we ask for triangulate as it will be easier to process

	//read obj
	if (!reader.ParseFromFile(file_name, config))
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
		totalIndexNb += objShape[i].mesh._Indices.size();
	HeapMemory<uint32_t> _Indices{ totalIndexNb };

	//allocate for the vertices buffers (there may be more _Uvs or _Normals in the attribute, but it is always linked to a single vertex anyway)
	HeapMemory<float> pos{ totalIndexNb * 3u};
	HeapMemory<float> tex_coords{totalIndexNb *2u };
	HeapMemory<float> _Normals{ totalIndexNb * 3u };

	uint32_t indexOffset = 0;
	for (uint32_t i = 0; i < objShape.size(); i++)
	{
		uint32_t indexNb = objShape[i].mesh._Indices.size();

		//make the _Indices, texcoords and normal
		for (uint32_t j = 0; j < indexNb; j++)
		{
			const tinyobj::index_t& j_index = objShape[i].mesh._Indices[j];

			_Indices[j + indexOffset] = j + indexOffset;

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
				_Normals[3 * (j + indexOffset)] = objAttrib._Normals[3 * j_index.normal_index];
				_Normals[3 * (j + indexOffset) + 1] = objAttrib._Normals[3 * j_index.normal_index + 1];
				_Normals[3 * (j + indexOffset) + 2] = objAttrib._Normals[3 * j_index.normal_index + 2];
			}
		}

		// set the number of _Indices to allow drawing of mesh
		meshes[i]._indices_nb = indexNb;
		//set the offest in the _Indices buffer to draw the mesh
		meshes[i]._indices_offset = indexOffset;

		indexOffset += indexNb;
	}

	// 1. creating and uploading the position buffer
	CreateStaticBufferHandle(GAPI, meshes[0]._Positions, sizeof(vec3) * totalIndexNb);
	UploadStaticBufferHandle(GAPI, meshes[0]._Positions, (void*)*pos, sizeof(vec3) * totalIndexNb);

	// 2. creating and uploading the _Indices buffer (same index buffer is used for all of the meshes, but with different offset)
	CreateStaticBufferHandle(GAPI, meshes[0]._Indices, sizeof(uint32_t) * totalIndexNb, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	UploadStaticBufferHandle(GAPI, meshes[0]._Indices, (void*)*_Indices, sizeof(uint32_t) * totalIndexNb);

	// 3. creating and uploading the _Uvs buffer
	if (hasTexCoords)
	{
		CreateStaticBufferHandle(GAPI, meshes[0]._Uvs, sizeof(vec2) * totalIndexNb);
		UploadStaticBufferHandle(GAPI, meshes[0]._Uvs, (void*)*tex_coords, sizeof(vec2) * totalIndexNb);
	}

	// 4. creating and uploading the normal buffer
	if (hasNormals)
	{
		CreateStaticBufferHandle(GAPI, meshes[0]._Normals, sizeof(vec3) * totalIndexNb);
		UploadStaticBufferHandle(GAPI, meshes[0]._Normals, (void*)*_Normals, sizeof(vec3) * totalIndexNb);
	}

	//copy the unique vertex buffers inside all meshes, to use same buffers for all
	for (uint32_t i = 1; i < meshes.Nb(); i++)
	{
		meshes[i]._Positions = meshes[0]._Positions;
		meshes[i]._Indices	= meshes[0]._Indices;
		meshes[i]._Uvs		= meshes[0]._Uvs;
		meshes[i]._Normals	= meshes[0]._Normals;
		meshes[i].tangents	= meshes[0].tangents;
	}
#else
	//get the size of the array
	uint32_t nbVertices = objAttrib.vertices.size() / 3;

	//allocate for the vertices buffers (there may be more _Uvs or _Normals in the attribute, but it is always linked to a single vertex anyway)
	MultipleScopedMemory<float> tex_coords{ nbVertices * 2u };
	MultipleScopedMemory<float> _Normals{ nbVertices * 3u };

	uint32_t totalIndexNb = 0;
	for (uint32_t i = 0; i < objShape.size(); i++)
		totalIndexNb += objShape[i].mesh.indices.size();
	MultipleScopedMemory<uint32_t> indices{ totalIndexNb };

	uint32_t indexOffset = 0;
	for (uint32_t i = 0; i < objShape.size(); i++)
	{
		uint32_t indexNb = objShape[i].mesh.indices.size();

		//make the _Indices, texcoords and normal
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
				_Normals[3 * j_index.vertex_index]	= objAttrib.normals[3 * j_index.normal_index];
				_Normals[3 * j_index.vertex_index + 1] = objAttrib.normals[3 * j_index.normal_index + 1];
				_Normals[3 * j_index.vertex_index + 2] = objAttrib.normals[3 * j_index.normal_index + 2];
			}
		}

		// set the number of _Indices to allow drawing of mesh
		meshes[i]._indices_nb = indexNb;
		//set the offest in the _Indices buffer to draw the mesh
		meshes[i]._indices_offset = indexOffset;
		meshes[i]._indices_type = VK_INDEX_TYPE_UINT32;

		indexOffset += indexNb;
	}


	// 1. creating and uploading the position buffer
	StaticBufferHandle posBuffer;
	CreateStaticBufferHandle(VulkanUploader, posBuffer, sizeof(vec3) * nbVertices);
	UploadStaticBufferHandle(VulkanUploader, posBuffer, (void*)objAttrib.vertices.data(), sizeof(vec3) * nbVertices);

	// 2. creating and uploading the _Indices buffer (same index buffer is used for all of the meshes, but with different offset)
	StaticBufferHandle indexBuffer;
	CreateStaticBufferHandle(VulkanUploader, indexBuffer, sizeof(uint32_t)* totalIndexNb, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	UploadStaticBufferHandle(VulkanUploader, indexBuffer, (void*)*indices, sizeof(uint32_t) * totalIndexNb);

	// 3. creating and uploading the _Uvs buffer
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
		UploadStaticBufferHandle(VulkanUploader, normalBuffer, (void*)*_Normals, sizeof(vec3) * nbVertices);
	}

	//copy the unique vertex buffers inside all meshes, to use same buffers for all
	for (uint32_t i = 0; i < meshes.Nb(); i++)
	{
		meshes[i]._VertexMemoryHandle.Alloc(4);
		meshes[i]._Positions				= posBuffer._StaticGPUBuffer;
		meshes[i]._VertexMemoryHandle[0]	= posBuffer._StaticGPUMemoryHandle;
		meshes[i]._Indices					= indexBuffer._StaticGPUBuffer;
		meshes[i]._VertexMemoryHandle[1]	= indexBuffer._StaticGPUMemoryHandle;
		meshes[i]._Uvs						= uvBuffer._StaticGPUBuffer;
		meshes[i]._VertexMemoryHandle[2]	= uvBuffer._StaticGPUMemoryHandle;
		meshes[i]._Normals					= normalBuffer._StaticGPUBuffer;
		meshes[i]._VertexMemoryHandle[3]	= normalBuffer._StaticGPUMemoryHandle;
		//meshes[i].tangents				= ;
		//meshes[i]._VertexMemoryHandle[4]
	}
#endif

}

void VulkanHelper::ClearMesh(const VkDevice& VulkanDevice, Mesh& mesh)
{
	VK_CLEAR_RAW_ARRAY_NO_FREE(mesh._VertexBuffers, 4, vkDestroyBuffer, VulkanDevice);
	if (mesh._Indices != nullptr)
		vkDestroyBuffer(VulkanDevice, mesh._Indices, nullptr);
	VK_CLEAR_ARRAY(mesh._VertexMemoryHandle, mesh._VertexMemoryHandle.Nb(), vkFreeMemory, VulkanDevice);
}


bool LoadGLTFBuffersInGPU(Uploader& VulkanUploader, const tinygltf::Model& loadedModel, Model& model)
{
	bool noError = true;

	//loading every buffer into device memory
	model._BuffersHandle.Alloc(loadedModel.buffers.size());
	for (uint32_t i = 0; i < loadedModel.buffers.size(); i++)
	{
		const tinygltf::Buffer& buffer = loadedModel.buffers[i];

		//we're using a static because it is easier to upload with, but we actually only want to allocate and send data to device memory
		StaticBufferHandle tmpBufferHandle;
		noError &= CreateVulkanBuffer(VulkanUploader, buffer.data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tmpBufferHandle._StaticGPUBuffer, tmpBufferHandle._StaticGPUMemoryHandle, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
		noError &= UploadStaticBufferHandle(VulkanUploader, tmpBufferHandle, buffer.data.data(), buffer.data.size());

		model._BuffersHandle[i] = tmpBufferHandle._StaticGPUMemoryHandle;

		vkDestroyBuffer(VulkanUploader._VulkanDevice, tmpBufferHandle._StaticGPUBuffer, nullptr);
	}

	return noError;
}

bool LoadGLTFMaterialInGPU(Uploader& VulkanUploader, const tinygltf::Model& loadedModel, Model& model)
{
	bool noError = true;
	VkResult result = VK_SUCCESS;

	//loading every image into device memory
	model._Textures.Alloc(loadedModel.images.size());
	for (uint32_t i = 0; i < loadedModel.images.size(); i++)
	{
		const tinygltf::Image& image = loadedModel.images[i];

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
                printf("error when loading %s, with %d channels.\n is the path correct?", image.uri.c_str(), image.component);
			return false;
		}

		LoadTexture(VulkanUploader, (void*)image.image.data(), (uint32_t)image.width, (uint32_t)image.height, imageFormat, model._Textures[i]);
	}

	model._Samplers.Alloc(loadedModel.samplers.size());
	for (uint32_t i = 0; i < loadedModel.samplers.size(); i++)
	{
		const tinygltf::Sampler& sampler = loadedModel.samplers[i];

		//create a sampler that can be used to sample this texture inside the shader
		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

		switch (sampler.magFilter)
		{
			case TINYGLTF_TEXTURE_FILTER_NEAREST :
				samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
				samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR :
				samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST :
				samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
				samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST :
				samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
				break;
			case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR :
				samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
				samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				break;
			case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR :
				samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
				samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
				break;
		}

		switch (sampler.minFilter)
		{
		case TINYGLTF_TEXTURE_FILTER_NEAREST:
			samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case TINYGLTF_TEXTURE_FILTER_LINEAR:
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
			samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			break;
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
			samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			break;
		}
		switch (sampler.wrapS)
		{
		case TINYGLTF_TEXTURE_WRAP_REPEAT:
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			break;
		case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			break;
		};

		switch (sampler.wrapT)
		{
		case TINYGLTF_TEXTURE_WRAP_REPEAT:
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			break;
		case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			break;
		case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			break;
		};

		VK_CALL_PRINT(vkCreateSampler(VulkanUploader._VulkanDevice, &samplerCreateInfo, nullptr, &model._Samplers[i]))
	}

	model._Materials.Alloc(loadedModel.materials.size());
	for (uint32_t i = 0; i < loadedModel.materials.size(); i++)
	{
		const tinygltf::Material& material = loadedModel.materials[i];
		Material& mat = model._Materials[i];

		uint32_t nb = (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
			+ (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
			+ (material.normalTexture.index >= 0);
		mat._Textures.Alloc(nb);

		uint32_t texIndex = 0;
		if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
		{
			const tinygltf::Texture& texture = loadedModel.textures[material.pbrMetallicRoughness.baseColorTexture.index];

			model._Textures[texture.source]._Sampler = model._Samplers[texture.sampler];
			model._Materials[i]._Textures[texIndex++] = model._Textures[texture.source];
		}
		if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
		{
			const tinygltf::Texture& texture = loadedModel.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index];

			model._Textures[texture.source]._Sampler = model._Samplers[texture.sampler];
			model._Materials[i]._Textures[texIndex++] = model._Textures[texture.source];
		}
		if (material.normalTexture.index >= 0)
		{
			const tinygltf::Texture& texture = loadedModel.textures[material.normalTexture.index];

			model._Textures[texture.source]._Sampler = model._Samplers[texture.sampler];
			model._Materials[i]._Textures[texIndex++] = model._Textures[texture.source];
		}
	}

	return noError;
}

bool VulkanHelper::LoadGLTFFile(Uploader& VulkanUploader, const char* file_name, Model& model)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model loadedModel;
	std::string err;
	std::string warnings;

	//first load model
	if (!loader.LoadASCIIFromFile(&loadedModel, &err, &warnings, file_name))
	{
		printf("error when reading %s :\nerror : %s\nwarning : %s", file_name, err.c_str(), warnings.c_str());
		return false;
	}

	//1. loading every buffer into device memory
	if (!LoadGLTFBuffersInGPU(VulkanUploader, loadedModel, model))
	{
		printf("error loading %s's buffers in GPU.", file_name);
		return false;
	}

	//2. load every image in memory
	if (!LoadGLTFMaterialInGPU(VulkanUploader, loadedModel, model))
	{
		printf("error loading %s's buffers in GPU.", file_name);
		return false;
	}

	uint32_t totalMeshNb = 0;
	for (uint32_t i = 0; i < loadedModel.meshes.size(); i++)
		totalMeshNb += loadedModel.meshes[i].primitives.size();
	model._Meshes.Alloc(totalMeshNb);
	model._material_index.Alloc(totalMeshNb);

	//2. creating vulkan buffer interface for meshes
	uint32_t meshIndex = 0;
	for (uint32_t i = 0; i < loadedModel.scenes[loadedModel.defaultScene].nodes.size(); i++)
	{
		uint32_t nodeIndex = loadedModel.scenes[loadedModel.defaultScene].nodes[i];
		tinygltf::Mesh& indexMesh = loadedModel.meshes[nodeIndex];

		for (uint32_t j = 0; j < indexMesh.primitives.size(); j++)
		{
			tinygltf::Primitive& jPrimitive = indexMesh.primitives[j];
			model._material_index[j] = jPrimitive.material;

			//create _Indices buffer from preallocated device memory
			{
				tinygltf::Accessor&	accessor		= loadedModel.accessors[jPrimitive.indices];
				tinygltf::BufferView& bufferView	= loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, (bufferView.byteLength - accessor.byteOffset), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Indices, model._BuffersHandle[bufferView.buffer], (bufferView.byteOffset + accessor.byteOffset), false);

				model._Meshes[meshIndex]._indices_nb = accessor.count;
				model._Meshes[meshIndex]._indices_offset = 0;

				switch (accessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					model._Meshes[meshIndex]._indices_type = VK_INDEX_TYPE_UINT8_KHR;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					model._Meshes[meshIndex]._indices_type = VK_INDEX_TYPE_UINT16;
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					model._Meshes[meshIndex]._indices_type = VK_INDEX_TYPE_UINT32;
					break;
				}
			}

			//position
			if (jPrimitive.attributes.count("POSITION") > 0)
			{
				tinygltf::Accessor& accessor		= loadedModel.accessors[jPrimitive.attributes["POSITION"]];
				tinygltf::BufferView& bufferView	= loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Positions, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._pos_offset = accessor.byteOffset + bufferView.byteOffset;
			}

			//_Uvs
			if (jPrimitive.attributes.count("TEXCOORD_0") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["TEXCOORD_0"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Uvs, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._uv_offset = bufferView.byteOffset + accessor.byteOffset;
			}

			//_Normals
			if (jPrimitive.attributes.count("NORMAL") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["NORMAL"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Normals, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._normal_offset = bufferView.byteOffset + accessor.byteOffset;
			}

			//tangents
			if (jPrimitive.attributes.count("TANGENT") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["TANGENT"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBuffer(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Tangents, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._tangent_offset = bufferView.byteOffset + accessor.byteOffset;
			}

			meshIndex++;
		}
	}

	return true;
}

void VulkanHelper::ClearModel(const VkDevice& VulkanDevice, Model& model)
{
	for (uint32_t i = 0; i < model._Meshes.Nb(); i++)
	{
		ClearMesh(VulkanDevice, model._Meshes[i]);
	}
	model._Meshes.Clear();
	model._material_index.Clear();
	VK_CLEAR_ARRAY(model._BuffersHandle, model._BuffersHandle.Nb(), vkFreeMemory, VulkanDevice);
	for (uint32_t i = 0; i < model._Textures.Nb(); i++)
	{
		ClearTexture(VulkanDevice, model._Textures[i]);
	}
	model._Textures.Clear();
	VK_CLEAR_ARRAY(model._Samplers, model._Samplers.Nb(), vkDestroySampler, VulkanDevice);
	for (uint32_t i = 0; i < model._Materials.Nb(); i++)
	{
		model._Materials[i]._Textures.Clear();
	}
	model._Materials.Clear();
}

/* Images/Textures */


bool VulkanHelper::CreateImage(Uploader& VulkanUploader, VkImage& imageToMake, VkDeviceMemory& imageMemory, uint32_t width, uint32_t height, uint32_t depth, VkImageType imagetype, VkFormat format, VkImageUsageFlags usageFlags, VkMemoryPropertyFlagBits memoryProperties, uint32_t offset, bool allocate_memory)
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
	imageInfo.extent.depth	= depth;//kept it in case we have 3D _Textures
	imageInfo.mipLevels		= 1u;
	imageInfo.arrayLayers	= 1u;
	imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL;//will only load images from buffers, so no need to change this
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//same this can be changed later and each scene will do what they need on their own
	imageInfo.usage			= usageFlags;//this is slighlty more important as we may use image as deffered rendering buffers, so made it available
	imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;//we have to command queues : one for the app, one for the UI. the UI shouldn't use iamge we create.
	imageInfo.samples		= VK_SAMPLE_COUNT_1_BIT;//why would you want more than one sample in a simple app ?
	imageInfo.flags			= 0; // Optional

	VK_CALL_PRINT(vkCreateImage(VulkanUploader._VulkanDevice, &imageInfo, nullptr, &imageToMake));

	if (allocate_memory)
	{
		//getting the necessary requirements to create our image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(VulkanUploader._VulkanDevice, imageToMake, &memRequirements);

		//trying to find a matching memory type between what the app wants and the device's limitation.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = GetMemoryTypeFromRequirements(memoryProperties, memRequirements, VulkanUploader._MemoryProperties);

		//allocating and associate the memory to our image.
		VK_CALL_PRINT(vkAllocateMemory(VulkanUploader._VulkanDevice, &allocInfo, nullptr, &imageMemory));
	}

	VK_CALL_PRINT(vkBindImageMemory(VulkanUploader._VulkanDevice, imageToMake, imageMemory, offset))

	return result == VK_SUCCESS;
}

bool VulkanHelper::UploadImage(Uploader& VulkanUploader, VkImage& imageToUploadTo, void* image_content, uint32_t width, uint32_t height, uint32_t channels, uint32_t depth)
{
	VkResult result = VK_SUCCESS;
	uint32_t size = width * height * depth * channels;
	void* CPUHandle;

	// create a staging buffer that can hold the images data
	VkBuffer tempBuffer;
	VkDeviceMemory tempMemory;
	CreateVulkanBuffer(VulkanUploader, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tempBuffer, tempMemory);

	// first copy the data in the CPU memory allocated by Vulkan for transfer
	VK_CALL_PRINT(vkMapMemory(VulkanUploader._VulkanDevice, tempMemory, 0, size, 0, &CPUHandle));
	memcpy(CPUHandle, image_content, size);
	vkUnmapMemory(VulkanUploader._VulkanDevice, tempMemory);

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
	vkCmdPipelineBarrier(VulkanUploader._CopyBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	// copying from staging buffer to image
	VkBufferImageCopy copyRegion{};
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageExtent.width	= width;
	copyRegion.imageExtent.height	= height;
	copyRegion.imageExtent.depth	= depth;
	vkCmdCopyBufferToImage(VulkanUploader._CopyBuffer, tempBuffer, imageToUploadTo, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	// free allocated memory and buffer
	VulkanUploader._ToFreeBuffers.Add(tempBuffer);
	VulkanUploader._ToFreeMemory.Add(tempMemory);

	return result == VK_SUCCESS;
}



bool VulkanHelper::LoadTexture(Uploader& VulkanUploader, const char* file_name, Texture& texture)
{
	VkResult result = VK_SUCCESS;

	// first loading the texture
	int32_t width, height, channels{0};
	stbi_uc* pixels = stbi_load(file_name, &width, &height, &channels, STBI_rgb_alpha);

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
		printf("error when loading %s,  with %d channels.\n is the path correct?", file_name, channels);
		return false;
	}

	LoadTexture(VulkanUploader, (void*)pixels, width, height, imageFormat, texture);
	stbi_image_free(pixels);

	return result == VK_SUCCESS;
}

#include "vulkan/utility/vk_format_utils.h"

bool VulkanHelper::LoadTexture(Uploader& VulkanUploader, const void* pixels, uint32_t width, uint32_t height, VkFormat imageFormat, Texture& texture)
{
	VkResult result = VK_SUCCESS;

	if (pixels == nullptr)
		return false;

	// create a staging buffer taht can hold the images data
	VkBuffer tempBuffer;
	VkDeviceMemory tempMemory;

	CreateVulkanBuffer(VulkanUploader, width * height * vkuFormatElementSize(imageFormat), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tempBuffer, tempMemory);

	//creating image from stbi info
	if (!CreateImage(VulkanUploader, texture._Image, texture._ImageMemory, width, height, 1u, VK_IMAGE_TYPE_2D, imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		return false;
	}

	//upload image to buffer
	if (!UploadImage(VulkanUploader, texture._Image, (void*)pixels, width, height, vkuGetFormatInfo(imageFormat).component_count))
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
	barrier.image = texture._Image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;// making the image accessible for ...
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // shaders

	//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
	vkCmdPipelineBarrier(VulkanUploader._CopyBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	//create an image view associated with the created image to make it available in shader
	VkImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.format = imageFormat;
	viewCreateInfo.image = texture._Image;
	viewCreateInfo.subresourceRange.layerCount = 1;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

	VK_CALL_PRINT(vkCreateImageView(VulkanUploader._VulkanDevice, &viewCreateInfo, nullptr, &texture._ImageView));

	////create a sampler that can be used to sample this texture inside the shader
	//VkSamplerCreateInfo samplerCreateInfo{};
	//samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	//samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	//samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	//
	//VK_CALL_PRINT(vkCreateSampler(VulkanUploader._VulkanDevice, &samplerCreateInfo, nullptr, &texture.sampler))

	return result == VK_SUCCESS;
}


void VulkanHelper::ClearTexture(const VkDevice& VulkanDevice, Texture& texture)
{
	//first release memory
	if (texture._Image != nullptr)
		vkDestroyImage(VulkanDevice, texture._Image, nullptr);
	if (texture._ImageMemory != nullptr)
		vkFreeMemory(VulkanDevice, texture._ImageMemory, nullptr);
	if (texture._ImageView != nullptr)
		vkDestroyImageView(VulkanDevice, texture._ImageView, nullptr);

	//then memset 0
	texture._Image = nullptr;
	texture._ImageMemory = nullptr;
	texture._ImageView = nullptr;
}

/* Acceleration Structures */

//creating a bottom level AS for each mesh of model
bool VulkanHelper::CreateRaytracedGeometryFromMesh(Uploader& VulkanUploader, RaytracedGeometry& raytracedGeometry, const VolatileLoopArray<Mesh>& mesh)
{
	//if an error happens...
	VkResult result = VK_SUCCESS;

	//preparing the struct we'll use in the loop

	//used to get back the GPU adress of buffers
	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;

	//used to build the Acceleration structure
	VkAccelerationStructureGeometryTrianglesDataKHR bottomLevelMeshASData{};
	bottomLevelMeshASData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

	//will be used to create the acceleration structure
	VkAccelerationStructureCreateInfoKHR ASCreateInfo{};
	ASCreateInfo.sType	= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	ASCreateInfo.type	= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

	//Note QB : this makes quite a lot of stack memory allocated, but it would be quite slow to allocate heap memory for this

	//array for the actual geometry description (in our case triangles)
	MultipleVolatileMemory<VkAccelerationStructureGeometryKHR>			bottomLevelMeshAS{ (VkAccelerationStructureGeometryKHR*)alloca(sizeof(VkAccelerationStructureGeometryKHR) * mesh.Nb()) };
	memset(*bottomLevelMeshAS, 0, sizeof(VkAccelerationStructureGeometryKHR) * mesh.Nb());
	//array for the build requests (one for each mesh)
	MultipleVolatileMemory<VkAccelerationStructureBuildGeometryInfoKHR> bottomLevelMeshASInfo{ (VkAccelerationStructureBuildGeometryInfoKHR*)alloca(sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * mesh.Nb()) };
	memset(*bottomLevelMeshASInfo, 0, sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * mesh.Nb());
	//the extent of the buffer of each meshes' geometry
	MultipleVolatileMemory<VkAccelerationStructureBuildRangeInfoKHR>	bottomLevelMeshASRangeInfo{ (VkAccelerationStructureBuildRangeInfoKHR*)alloca(sizeof(VkAccelerationStructureBuildRangeInfoKHR) * mesh.Nb()) };
	memset(*bottomLevelMeshASRangeInfo, 0, sizeof(VkAccelerationStructureBuildRangeInfoKHR) * mesh.Nb());
	MultipleVolatileMemory<VkAccelerationStructureBuildRangeInfoKHR*>	bottomLevelMeshASRangeInfoPtr{ (VkAccelerationStructureBuildRangeInfoKHR**)alloca(sizeof(VkAccelerationStructureBuildRangeInfoKHR*) * mesh.Nb()) };
	memset(*bottomLevelMeshASRangeInfo, 0, sizeof(VkAccelerationStructureBuildRangeInfoKHR) * mesh.Nb());

	//allocating buffer and memory beforehand, as there will be one acceleration structure for each mesh
	raytracedGeometry._AccelerationStructure.Alloc(mesh.Nb());
	raytracedGeometry._AccelerationStructureBuffer.Alloc(mesh.Nb());
	raytracedGeometry._AccelerationStructureMemory.Alloc(mesh.Nb());

	//describing gemetry and creating necessarry buyffer for each mesh
	for (uint32_t i = 0; i < mesh.Nb(); i++)
	{
		//the current mesh we'll work on
		const VulkanHelper::Mesh& iMesh = mesh[i];

		//getting back the positions' GPU address
		addressInfo.buffer = iMesh._Positions;
		VkDeviceAddress GPUBufferAddress = vkGetBufferDeviceAddress(VulkanUploader._VulkanDevice, &addressInfo);

		//giving the position data
		bottomLevelMeshASData.vertexData	= VkDeviceOrHostAddressConstKHR{ GPUBufferAddress };// this is sure to be a static buffer as we will not change vertex data
		bottomLevelMeshASData.vertexFormat	= VK_FORMAT_R32G32B32_SFLOAT;//pos is vec3
		bottomLevelMeshASData.vertexStride	= sizeof(vec3);//pos is vec3
		//we do not have the data to know "bottomLevelMeshAS.maxVertex"

		//getting back the indices' GPU address
		addressInfo.buffer = iMesh._Indices;
		GPUBufferAddress = vkGetBufferDeviceAddress(VulkanUploader._VulkanDevice, &addressInfo);

		//giving the indices
		bottomLevelMeshASData.indexData = VkDeviceOrHostAddressConstKHR{ GPUBufferAddress };// this is sure to be a static buffer as we will not change vertex data
		bottomLevelMeshASData.indexType = iMesh._indices_type;

		//we now have a proper triangle geometry
		VkAccelerationStructureGeometryKHR& meshAS = bottomLevelMeshAS[i];
		meshAS.sType		= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		meshAS.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		meshAS.flags		= VK_GEOMETRY_OPAQUE_BIT_KHR;
		meshAS.geometry.triangles = bottomLevelMeshASData;

		//for each triangle geometry description, we want to build
		VkAccelerationStructureBuildGeometryInfoKHR& meshASBuild = bottomLevelMeshASInfo[i];
		meshASBuild.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		meshASBuild.type			= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;//we are specifying a geometry, this makes it a bottom level
		meshASBuild.pGeometries		= &meshAS;
		meshASBuild.geometryCount	= 1;
		meshASBuild.mode			= VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;//we are creating it for the fist (and propably last) time

		//setting the bounds of our mesh (as when loading from models, the buffer for multiple geometry may have been the same)
		VkAccelerationStructureBuildRangeInfoKHR& meshASRange = bottomLevelMeshASRangeInfo[i];
		meshASRange.firstVertex		= iMesh._pos_offset;
		meshASRange.primitiveOffset = iMesh._indices_offset * 3;
		meshASRange.primitiveCount	= iMesh._indices_nb / 3;
		meshASRange.transformOffset = 0;
		bottomLevelMeshASRangeInfoPtr[i] = &meshASRange;

		//the size the AS buffer will need to be considering the geometry we've referenced above
		VkAccelerationStructureBuildSizesInfoKHR buildSize{};
		buildSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		VK_CALL_KHR(VulkanUploader._VulkanDevice, vkGetAccelerationStructureBuildSizesKHR, VulkanUploader._VulkanDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &meshASBuild, &meshASRange.primitiveCount, &buildSize);

		//unfortunately, we cannot use the same memory for all the acceleration structure buffer, as size may defer between acceleration structure
		VulkanHelper::CreateVulkanBuffer(VulkanUploader, buildSize.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			raytracedGeometry._AccelerationStructureBuffer[i], raytracedGeometry._AccelerationStructureMemory[i], 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

		//setting our newly created buffer to create our acceleration structure object
		ASCreateInfo.buffer = raytracedGeometry._AccelerationStructureBuffer[i];
		ASCreateInfo.size	= buildSize.accelerationStructureSize;
		//creating a new acceleration structure
		VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCreateAccelerationStructureKHR, VulkanUploader._VulkanDevice, &ASCreateInfo, nullptr, &raytracedGeometry._AccelerationStructure[i]);

		//set our new Acceleration Structure Object to be the one receiving the geometry we've specified
		meshASBuild.dstAccelerationStructure = raytracedGeometry._AccelerationStructure[i];

		//to create the acceleration structure, vulkan needs a temporary copy buffer.
		//fortunately we just have to give it a buffer and it handles everything else
		VkBuffer scratchBuffer;
		VkDeviceMemory scratchMemory;
		VulkanHelper::CreateVulkanBuffer(VulkanUploader, buildSize.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			scratchBuffer, scratchMemory, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

		//get the address of our newly created buffer and give it to our build data
		addressInfo.buffer = scratchBuffer;
		GPUBufferAddress = vkGetBufferDeviceAddress(VulkanUploader._VulkanDevice, &addressInfo);
		meshASBuild.scratchData = VkDeviceOrHostAddressKHR{ GPUBufferAddress };

		//do not forget to free teh temporary buffer
		VulkanUploader._ToFreeBuffers.Add(scratchBuffer);
		VulkanUploader._ToFreeMemory.Add(scratchMemory);
	}

	//finally, asking to build all of the acceleration structures at once (this is basically a copy command, so it is a defered call)
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCmdBuildAccelerationStructuresKHR, VulkanUploader._CopyBuffer, mesh.Nb(), *bottomLevelMeshASInfo, *bottomLevelMeshASRangeInfoPtr);

	return result == VK_SUCCESS;
}

void VulkanHelper::ClearRaytracedGeometry(const VkDevice& VulkanDevice, RaytracedGeometry& raytracedGeometry)
{
	VK_CLEAR_ARRAY(raytracedGeometry._AccelerationStructureBuffer, raytracedGeometry._AccelerationStructureBuffer.Nb(), vkDestroyBuffer, VulkanDevice);
	VK_CLEAR_ARRAY(raytracedGeometry._AccelerationStructureMemory, raytracedGeometry._AccelerationStructureMemory.Nb(), vkFreeMemory, VulkanDevice);
}

bool VulkanHelper::UploadRaytracedModelFromGeometry(Uploader& VulkanUploader, RaytracedModel& raytracedModel, const mat4& transform, const RaytracedGeometry& geometry, bool isUpdate)
{
	//to record if an error happened
	VkResult result = VK_SUCCESS;

	// preparing the struct we'll use in the loop

	//used to get back the GPU adress of buffers
	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;

	//The number of BLAS in our single TLAS
	VkAccelerationStructureBuildRangeInfoKHR* buildRange = static_cast<VkAccelerationStructureBuildRangeInfoKHR*>(alloca(sizeof(VkAccelerationStructureBuildRangeInfoKHR)));
	memset(buildRange, 0, sizeof(VkAccelerationStructureBuildRangeInfoKHR));
	buildRange->primitiveCount = geometry._AccelerationStructure.Nb();

	//to create the acceleration structure, we need a temporary buffer to put the instances
	VkBuffer		tmpBuffer;
	VkDeviceMemory	tmpMemory;
	VkDeviceSize	tmpSize = sizeof(VkAccelerationStructureInstanceKHR) * geometry._AccelerationStructure.Nb();
	//creating temp buffer
	VulkanHelper::CreateVulkanBuffer(VulkanUploader, tmpSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		tmpBuffer, tmpMemory, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

	//the description of the Top level Acceleration Structure
	MultipleVolatileMemory<VkAccelerationStructureInstanceKHR> ASInstances{ (VkAccelerationStructureInstanceKHR*)alloca(sizeof(VkAccelerationStructureInstanceKHR) * geometry._AccelerationStructure.Nb()) };
	memset(*ASInstances, 0, sizeof(VkAccelerationStructureInstanceKHR) * geometry._AccelerationStructure.Nb());
	//the instances of TLAS
	MultipleVolatileMemory<VkAccelerationStructureGeometryKHR> instancesAS{ (VkAccelerationStructureGeometryKHR*)alloca(sizeof(VkAccelerationStructureGeometryKHR) * geometry._AccelerationStructure.Nb()) };
	memset(*instancesAS, 0, sizeof(VkAccelerationStructureGeometryKHR) * geometry._AccelerationStructure.Nb());
	
	//looping through all bottom level gemetry to create the associated instances
	for (uint32_t i = 0; i < geometry._AccelerationStructure.Nb(); i++)
	{
		//for each instance, 
		VkAccelerationStructureInstanceKHR& iASInstance = ASInstances[i];

		iASInstance.mask = 0xFF;//every instance will be hittable
		iASInstance.transform = { transform[0], transform[1], transform[2], transform[3],
								transform[4], transform[5], transform[6], transform[7],
								transform[8], transform[9], transform[10],  transform[11] };//changing transform
		iASInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	

		//getting back the indices' GPU address
		addressInfo.buffer = geometry._AccelerationStructureBuffer[i];
		VkDeviceAddress GPUBufferAddress = vkGetBufferDeviceAddress(VulkanUploader._VulkanDevice, &addressInfo);

		//and associating instance with BLAS
		iASInstance.accelerationStructureReference = GPUBufferAddress;

		//getting back the address
		addressInfo.buffer = tmpBuffer;
		GPUBufferAddress = vkGetBufferDeviceAddress(VulkanUploader._VulkanDevice, &addressInfo);

		//set our instances data 
		VkAccelerationStructureGeometryInstancesDataKHR ASInstancesData{};
		ASInstancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		ASInstancesData.data = VkDeviceOrHostAddressConstKHR{ GPUBufferAddress + i * sizeof(VkAccelerationStructureInstanceKHR) };

		//all the instances make one model's "geometry"
		VkAccelerationStructureGeometryKHR& instancesASGeom = instancesAS[i];
		instancesASGeom.sType				= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		instancesASGeom.geometryType		= VK_GEOMETRY_TYPE_INSTANCES_KHR;
		instancesASGeom.flags				= VK_GEOMETRY_OPAQUE_BIT_KHR;
		instancesASGeom.geometry.instances	= ASInstancesData;

	}

	// copy to the temporary buffer
	void* tmpHandle;
	VK_CALL_PRINT(vkMapMemory(VulkanUploader._VulkanDevice, tmpMemory, 0, tmpSize, 0, &tmpHandle));
	memcpy(tmpHandle, *ASInstances, tmpSize);
	vkUnmapMemory(VulkanUploader._VulkanDevice, tmpMemory);

	
	//building all the instances into a single top level AS
	VkAccelerationStructureBuildGeometryInfoKHR instanceASBuild{};
	instanceASBuild.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	instanceASBuild.type			= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;//we are specifying an instance, so it is a top level
	instanceASBuild.pGeometries		= *instancesAS;
	instanceASBuild.geometryCount	= 1;
	instanceASBuild.mode			= isUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;//choose between creation and update


	VkAccelerationStructureBuildSizesInfoKHR buildSize{};
	buildSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkGetAccelerationStructureBuildSizesKHR, VulkanUploader._VulkanDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &instanceASBuild, &buildRange->primitiveCount, &buildSize);

	//we first need to create the Top Level AS Buffer
	if (!isUpdate)
	{
	
		//creating our acceleration structure buffer
		VulkanHelper::CreateVulkanBuffer(VulkanUploader, buildSize.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			raytracedModel._AccelerationStructureBuffer, raytracedModel._AccelerationStructureMemory);

		VkAccelerationStructureCreateInfoKHR instanceASCreateInfo{};
		instanceASCreateInfo.sType	= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		instanceASCreateInfo.type	= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		instanceASCreateInfo.buffer = raytracedModel._AccelerationStructureBuffer;
		instanceASCreateInfo.size	= buildSize.accelerationStructureSize;
		
		VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCreateAccelerationStructureKHR, VulkanUploader._VulkanDevice, &instanceASCreateInfo, nullptr, &raytracedModel._AccelerationStructure);
	}
	else
	{
		instanceASBuild.srcAccelerationStructure = raytracedModel._AccelerationStructure;
	}

	instanceASBuild.dstAccelerationStructure = raytracedModel._AccelerationStructure;

	//make the scratch buffer 
	VkBuffer		scratchBuffer;
	VkDeviceMemory	scratchMemory;
	VkDeviceSize	scratchSize = isUpdate ? buildSize.updateScratchSize : buildSize.buildScratchSize;
	VulkanHelper::CreateVulkanBuffer(VulkanUploader, scratchSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		scratchBuffer, scratchMemory, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);


	//getting back the address
	addressInfo.buffer = scratchBuffer;
	VkDeviceAddress GPUBufferAddress = vkGetBufferDeviceAddress(VulkanUploader._VulkanDevice, &addressInfo);

	instanceASBuild.scratchData = VkDeviceOrHostAddressKHR{GPUBufferAddress};


	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCmdBuildAccelerationStructuresKHR, VulkanUploader._CopyBuffer, 1, &instanceASBuild, &buildRange);

	//deffered clear of all temporary memory
	VulkanUploader._ToFreeBuffers.Add(scratchBuffer);
	VulkanUploader._ToFreeMemory.Add(scratchMemory);
	//VulkanUploader._ToFreeBuffers.Add(tmpBuffer);
	//VulkanUploader._ToFreeMemory.Add(tmpMemory);

	return result == VK_SUCCESS;
}

void VulkanHelper::ClearRaytracedModel(const VkDevice& VulkanDevice, RaytracedModel& raytracedModel)
{
	VK_CALL_KHR(VulkanDevice, vkDestroyAccelerationStructureKHR, VulkanDevice, raytracedModel._AccelerationStructure, nullptr);
	vkDestroyBuffer(VulkanDevice, raytracedModel._AccelerationStructureBuffer, nullptr);
	vkFreeMemory(VulkanDevice, raytracedModel._AccelerationStructureMemory, nullptr);
}
