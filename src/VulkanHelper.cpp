#include "VulkanHelper.h"

#include "GraphicsAPIManager.h"

//vulkan shader compiler
#include "shaderc/shaderc.h"

//vulkan helper
#include "vulkan/utility/vk_format_utils.h"

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

bool VulkanHelper::StartUploader(const GraphicsAPIManager& GAPI, Uploader& VulkanUploader)
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

	//this machine's specific GPU properties to get the SBT alignement and other stuff
	VulkanUploader._RTProperties = {};
	VulkanUploader._RTProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	//needed to get the above info
	VkPhysicalDeviceProperties2 GPUProperties{};
	GPUProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	GPUProperties.pNext = &VulkanUploader._RTProperties;
	vkGetPhysicalDeviceProperties2(GAPI._VulkanGPU, &GPUProperties);

	return result == VK_SUCCESS;
}

void VulkanHelper::StartUploader(const GAPIHandle& GAPIHandle, Uploader& VulkanUploader)
{

	//The Uploader will use the same command buffer and queues as the Runtime Handle
	VulkanUploader._VulkanDevice	= GAPIHandle._VulkanDevice;
	VulkanUploader._CopyQueue		= GAPIHandle._VulkanQueues[0];
	VulkanUploader._CopyBuffer		= GAPIHandle.GetCurrentVulkanCommand();
	VulkanUploader._CopyPool		= VK_NULL_HANDLE;

	VulkanUploader._MemoryProperties = {};
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

	//if there is a copy pool, it is a one time buffer we want to destroy
	if (VulkanUploader._CopyPool != VK_NULL_HANDLE)
		vkFreeCommandBuffers(VulkanUploader._VulkanDevice, VulkanUploader._CopyPool, 1, &VulkanUploader._CopyBuffer);
	else//this is a runtime upload, where we used an opened command, we need to make it available
	{
		vkResetCommandBuffer(VulkanUploader._CopyBuffer, 0);

		// beginning the copy command
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CALL_PRINT(vkBeginCommandBuffer(VulkanUploader._CopyBuffer, &beginInfo));
	}

	return result == VK_SUCCESS;
}

/*===== SHADERS =====*/

bool VulkanHelper::CompileVulkanShaders(Uploader& VulkanUploader, VkShaderModule& shader, VkShaderStageFlagBits shaderStage, const char* shader_source, const char* shader_name, const char* entry_point)
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

bool VulkanHelper::CreateVulkanShaders(Uploader& VulkanUploader, ShaderScripts& shader, VkShaderStageFlagBits shaderStage, const char* shader_source, const char* shader_name, const char* entry_point)
{
	//copying source for potential edit down the line
	uint32_t source_length = strlen(shader_source);
	shader._ShaderSource.Alloc(source_length + 1);
	ZERO_SET(shader._ShaderSource, (source_length + 1) *sizeof(char))
	memcpy((void*)*shader._ShaderSource, shader_source, source_length * sizeof(char));

	//copyinmg the name 
	uint32_t name_length = strlen(shader_name);
	shader._ShaderName.Alloc(name_length + 1);
	ZERO_SET(shader._ShaderName, (name_length + 1) * sizeof(char))
	memcpy((void*)*shader._ShaderName, shader_name, name_length * sizeof(char));

	//putting the stage in the struct to rememeber the kind of shader it is
	shader._ShaderStage = shaderStage;

	return CompileVulkanShaders(VulkanUploader, shader._ShaderModule, shader._ShaderStage, *shader._ShaderSource, *shader._ShaderName, entry_point);
}

void VulkanHelper::ClearVulkanShader(const VkDevice& VulkanDevice, ShaderScripts& shader)
{
	//free memory
	shader._ShaderSource.Clear();
	shader._ShaderName.Clear();

	//free shader module
	vkDestroyShaderModule(VulkanDevice, shader._ShaderModule,nullptr);
}

void VulkanHelper::MakePipelineShaderFromScripts(MultipleScopedMemory<VkPipelineShaderStageCreateInfo>& PipelineShaderStages, const List<ShaderScripts>& ShaderList)
{
	//the idea is that as this is a scoped array, we allocate here and it will be created before calling this function for it being freed at the end of the calling function scope
	PipelineShaderStages.Alloc(ShaderList.Nb());
	ZERO_SET(PipelineShaderStages, ShaderList.Nb() * sizeof(VkPipelineShaderStageCreateInfo));

	{
		List<ShaderScripts>::ListNode* indexedNode = ShaderList.GetHead();
		for (uint32_t i = 0; i < ShaderList.Nb(); i++)
		{
			VkPipelineShaderStageCreateInfo& iShaderStage = PipelineShaderStages[i];
			const ShaderScripts& iShaderScripts = indexedNode->data;

			iShaderStage.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			iShaderStage.module = iShaderScripts._ShaderModule;
			iShaderStage.stage	= iShaderScripts._ShaderStage;
			iShaderStage.pName	= "main";

			indexedNode = indexedNode->next;
		}
	}
}

bool VulkanHelper::GetShaderBindingTable(Uploader& VulkanUploader, const VkPipeline& VulkanPipeline,  ShaderBindingTable& SBT, uint32_t nbMissGroup, uint32_t nbHitGroup, uint32_t nbCallable)
{
	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	//the needed alignement to create our shader binding table
	uint32_t handleSize			= VulkanUploader._RTProperties.shaderGroupHandleSize;
	uint32_t handleAlignment	= VulkanUploader._RTProperties.shaderGroupHandleAlignment;
	uint32_t baseAlignment		= VulkanUploader._RTProperties.shaderGroupBaseAlignment;
	uint32_t handleSizeAligned	= VulkanHelper::AlignUp(handleSize, handleAlignment);
	uint32_t baseSizeAligned	= VulkanHelper::AlignUp(handleSizeAligned, baseAlignment);

	//set raygen shader's aligned stride and size (there is always only one)
	SBT._RayGenRegion.stride	= baseSizeAligned;
	SBT._RayGenRegion.size		= baseSizeAligned;

	//miss shader's aligned stride and size
	SBT._MissRegion.stride	= handleSizeAligned;
	SBT._MissRegion.size	= baseSizeAligned * nbMissGroup;

	//hit shader's aligned stride and size
	SBT._HitRegion.stride	= handleSizeAligned;
	SBT._HitRegion.size		= baseSizeAligned * nbHitGroup;

	//callable shader's aligned stride and size
	SBT._CallableRegion.stride	= handleSizeAligned;
	SBT._CallableRegion.size	= baseSizeAligned * nbCallable;

	//computing the total nb of shader groups (with raygen being one)
	uint32_t totalNbOfGroups = 1 + nbMissGroup + nbHitGroup + nbCallable;

	//creating a GPU buffer for the Shader Binding Table
	VulkanHelper::CreateVulkanBufferAndMemory(VulkanUploader, totalNbOfGroups * baseSizeAligned, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, SBT._SBTBuffer, SBT._SBTMemory, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

	//get back handle from shader group created in pipeline
	MultipleScopedMemory<uint8_t> shaderGroupHandle{ totalNbOfGroups * handleSize };
	uint8_t* shaderGroupHandleIter = *shaderGroupHandle;
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkGetRayTracingShaderGroupHandlesKHR, VulkanUploader._VulkanDevice, VulkanPipeline, 0, totalNbOfGroups, totalNbOfGroups * handleSize, *shaderGroupHandle);

	//mapping memory to buffer
	uint8_t* CPUSBTBufferMap;
	vkMapMemory(VulkanUploader._VulkanDevice, SBT._SBTMemory, 0, totalNbOfGroups * baseSizeAligned, 0, (void**)(&CPUSBTBufferMap));

	//first copy ray gen handle
	memcpy(CPUSBTBufferMap, shaderGroupHandleIter, handleSize);
	CPUSBTBufferMap += baseSizeAligned;
	shaderGroupHandleIter += handleSize;

	//copy of miss group
	memcpy(CPUSBTBufferMap, shaderGroupHandleIter, handleSize * nbMissGroup);
	CPUSBTBufferMap += baseSizeAligned * nbMissGroup;
	shaderGroupHandleIter += handleSize * nbMissGroup;
	
	//copy of hit group
	memcpy(CPUSBTBufferMap, shaderGroupHandleIter, handleSize * nbHitGroup);
	CPUSBTBufferMap += baseSizeAligned * nbHitGroup;
	shaderGroupHandleIter += handleSize * nbHitGroup;

	//copy of hit group
	memcpy(CPUSBTBufferMap, shaderGroupHandleIter, handleSize * nbCallable);

	//unmap and copy
	vkUnmapMemory(VulkanUploader._VulkanDevice, SBT._SBTMemory);

	//get the created gpu buffer's device address for the ray gen region
	VK_GET_BUFFER_ADDRESS(VulkanUploader._VulkanDevice, VkDeviceAddress, SBT._SBTBuffer, SBT._RayGenRegion.deviceAddress);

	//infer the adress of each shader group as they are copied sequentially
	SBT._MissRegion.deviceAddress		= SBT._RayGenRegion.deviceAddress + SBT._RayGenRegion.size;
	SBT._HitRegion.deviceAddress		= SBT._MissRegion.deviceAddress + SBT._MissRegion.size;
	SBT._CallableRegion.deviceAddress	= SBT._HitRegion.deviceAddress + SBT._HitRegion.size;

	return result == VK_SUCCESS;
}

void VulkanHelper::ClearShaderBindingTable(const VkDevice& VulkanDevice, ShaderBindingTable& SBT)
{
	if (SBT._SBTBuffer != nullptr)
		vkDestroyBuffer(VulkanDevice, SBT._SBTBuffer, nullptr);
	SBT._SBTBuffer = nullptr;
	if (SBT._SBTMemory != nullptr)
		vkFreeMemory(VulkanDevice, SBT._SBTMemory, nullptr);
	SBT._SBTMemory = nullptr;
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

bool VulkanHelper::AllocateVulkanMemory(const Uploader& VulkanUploader, VkMemoryPropertyFlags properties, const VkMemoryRequirements& memRequirements, uint32_t memoryType, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags flags)
{
	VkResult result = VK_SUCCESS;

	{
		//we have requirements like size we also should choose the type.
		//we choose the first type that will go with the properties user asked for 
		//(using the flags to get the better performing memory type)
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize	= AlignUp(memRequirements.size, memRequirements.alignment);//allocate the required size (add a few bytes if a particular alignement is needed)
		allocInfo.memoryTypeIndex	= memoryType;

		VkMemoryAllocateFlagsInfo flagsInfo{};
		if (flags != 0)
		{
			flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
			flagsInfo.flags = flags;
			allocInfo.pNext = &flagsInfo;
		}

		VK_CALL_PRINT(vkAllocateMemory(VulkanUploader._VulkanDevice, &allocInfo, nullptr, &bufferMemory))
	}

	return result == VK_SUCCESS;
}


void VulkanHelper::MemorySyncScope(VkCommandBuffer cmdBuffer, VkAccessFlagBits srcAccessMask, VkAccessFlagBits dstAccessMask, VkPipelineStageFlagBits syncScopeStart, VkPipelineStageFlagBits syncScopeEnd)
{
	VkMemoryBarrier barrier{};
	barrier.sType			= VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.srcAccessMask	= srcAccessMask;// previous accessibility and visibility
	barrier.dstAccessMask	= dstAccessMask; // wanted accessibility and visibility

	//put a synchronisation scope where all memory of a certain access mask must change to a different access mask between start and end of scope
	vkCmdPipelineBarrier(cmdBuffer, syncScopeStart, syncScopeEnd, 0, 1, &barrier, 0, nullptr, 0, nullptr);

}

bool VulkanHelper::CreateVulkanBuffer(const Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer)
{
	//to know if we succeeded
	VkResult result = VK_SUCCESS;

	//creating a buffer with info given in parameters
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType		= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size			= size;
	bufferInfo.usage		= usage;
	bufferInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;

	VK_CALL_PRINT(vkCreateBuffer(VulkanUploader._VulkanDevice, &bufferInfo, nullptr, &buffer));

	return result == VK_SUCCESS;
}

bool VulkanHelper::CreateVulkanBufferMemory(const Uploader& VulkanUploader, VkMemoryPropertyFlags properties, const VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags flags)
{
	//first let's get the requirements that the memory should follow (like it needs to be 4 bytes and such)
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(VulkanUploader._VulkanDevice, buffer, &memRequirements);
	//then allocate
	return AllocateVulkanMemory(VulkanUploader, properties, memRequirements, GetMemoryTypeFromRequirements(properties, memRequirements, VulkanUploader._MemoryProperties), bufferMemory, flags);
}


bool VulkanHelper::CreateVulkanBufferAndMemory(const Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, uint32_t offset, bool allocate_memory, VkMemoryAllocateFlags flags)
{
	//to know if we succeeded
	VkResult result = VK_SUCCESS;
	
	//Creates the Vulkan Buffer
	CreateVulkanBuffer(VulkanUploader, size, usage, buffer);
	
	//now that we have created our buffer object, we need to allocate GPU memory associated with it
	if (allocate_memory)
		CreateVulkanBufferMemory(VulkanUploader, properties, buffer, bufferMemory, flags);
	
	//then link together the buffer "interface" object, and the allcated memory
	VK_CALL_PRINT(vkBindBufferMemory(VulkanUploader._VulkanDevice, buffer, bufferMemory, offset));

	return result == VK_SUCCESS;
}

bool VulkanHelper::CreateTmpBufferAndAddress(Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceAddress& tmpBufferAddress, VkMemoryAllocateFlags flags)
{
	//creating the temporary buffer
	if (VulkanHelper::CreateVulkanBufferAndMemory(VulkanUploader, size, usage, properties, buffer, bufferMemory, 0, true, flags))
	{
		//get the address of our newly created buffer and give it to the addresss in parameters
		VK_GET_BUFFER_ADDRESS(VulkanUploader._VulkanDevice, VkDeviceAddress, buffer, tmpBufferAddress);

		//do not forget to free the temporary buffer
		VulkanUploader._ToFreeBuffers.Add(buffer);
		VulkanUploader._ToFreeMemory.Add(bufferMemory);

		return true;
	}

	return false;
}

bool VulkanHelper::CreateUniformBufferHandle(Uploader& VulkanUploader, UniformBufferHandle& bufferHandle, uint32_t bufferNb, VkDeviceSize size,
	VkMemoryPropertyFlags properties, VkBufferUsageFlags usage, bool map_cpu_memory_handle, VkMemoryAllocateFlags flags)

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
		result = CreateVulkanBufferAndMemory(VulkanUploader, size, usage, properties, bufferHandle._GPUBuffer[i], bufferHandle._GPUMemoryHandle[i], 0, true, flags) ? VK_SUCCESS : VK_ERROR_UNKNOWN;

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


bool VulkanHelper::CreateStaticBufferHandle(const Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, VkDeviceSize size, VkBufferUsageFlags staticBufferUsage, VkMemoryPropertyFlags staticBufferProperties, VkMemoryAllocateFlags flags)
{
	//creating the static buffer the definition of this implementation of a static buffer, is a device local buffer. tehrefore, it needs at least transfer dst and device local)
	return CreateVulkanBufferAndMemory(VulkanUploader, size, staticBufferUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, staticBufferProperties | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bufferHandle._StaticGPUBuffer, bufferHandle._StaticGPUMemoryHandle, 0, true, flags);
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
	CreateVulkanBufferAndMemory(VulkanUploader, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingGPUMemoryHandle);

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
		meshes[i]._indices_offset = indexOffset * sizeof(uint32_t);
		meshes[i]._indices_type = VK_INDEX_TYPE_UINT32;

		//sets the nb of vertices in the buffers
		meshes[i]._pos_nb		= nbVertices;
		meshes[i]._uv_nb		= nbVertices;
		meshes[i]._normal_nb	= nbVertices;
		meshes[i]._tangent_nb	= nbVertices;

		indexOffset += indexNb;
	}


	// 1. creating and uploading the position buffer
	StaticBufferHandle posBuffer;
	CreateStaticBufferHandle(VulkanUploader, posBuffer, sizeof(vec3) * nbVertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 
																				| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
																				| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
																				| VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
																				0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
	UploadStaticBufferHandle(VulkanUploader, posBuffer, (void*)objAttrib.vertices.data(), sizeof(vec3) * nbVertices);

	// 2. creating and uploading the _Indices buffer (same index buffer is used for all of the meshes, but with different offset)
	StaticBufferHandle indexBuffer;
	CreateStaticBufferHandle(VulkanUploader, indexBuffer, sizeof(uint32_t)* totalIndexNb, VK_BUFFER_USAGE_INDEX_BUFFER_BIT 
																						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
																						| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
																						| VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
																						0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
	UploadStaticBufferHandle(VulkanUploader, indexBuffer, (void*)*indices, sizeof(uint32_t) * totalIndexNb);

	// 3. creating and uploading the _Uvs buffer
	StaticBufferHandle uvBuffer;
	if (hasTexCoords)
	{
		CreateStaticBufferHandle(VulkanUploader, uvBuffer, sizeof(vec2) * nbVertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 
																					| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
																					| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
																					| VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
																					0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
		UploadStaticBufferHandle(VulkanUploader, uvBuffer, (void*)*tex_coords, sizeof(vec2) * nbVertices);
	}

	// 4. creating and uploading the normal buffer
	StaticBufferHandle normalBuffer;
	if (hasNormals)
	{
		CreateStaticBufferHandle(VulkanUploader, normalBuffer, sizeof(vec3) * nbVertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 
																						| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
																						| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
																						| VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
																						0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
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
		noError &= CreateVulkanBufferAndMemory(VulkanUploader, buffer.data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tmpBufferHandle._StaticGPUBuffer, tmpBufferHandle._StaticGPUMemoryHandle, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
		noError &= UploadStaticBufferHandle(VulkanUploader, tmpBufferHandle, buffer.data.data(), buffer.data.size());

		model._BuffersHandle[i] = tmpBufferHandle._StaticGPUMemoryHandle;

        VulkanUploader._ToFreeBuffers.Add(tmpBufferHandle._StaticGPUBuffer);
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

				CreateVulkanBufferAndMemory(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Indices, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._indices_nb = accessor.count;
				model._Meshes[meshIndex]._indices_offset = bufferView.byteOffset + accessor.byteOffset;
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

				CreateVulkanBufferAndMemory(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Positions, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._pos_offset = accessor.byteOffset + bufferView.byteOffset;
				model._Meshes[meshIndex]._pos_nb = accessor.count;

			}

			//_Uvs
			if (jPrimitive.attributes.count("TEXCOORD_0") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["TEXCOORD_0"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBufferAndMemory(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Uvs, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._uv_offset = bufferView.byteOffset + accessor.byteOffset;
				model._Meshes[meshIndex]._uv_nb = accessor.count;

			}

			//_Normals
			if (jPrimitive.attributes.count("NORMAL") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["NORMAL"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBufferAndMemory(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Normals, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._normal_offset = bufferView.byteOffset + accessor.byteOffset;
				model._Meshes[meshIndex]._normal_nb = accessor.count;

			}

			//tangents
			if (jPrimitive.attributes.count("TANGENT") > 0)
			{
				tinygltf::Accessor& accessor = loadedModel.accessors[jPrimitive.attributes["TANGENT"]];
				tinygltf::BufferView& bufferView = loadedModel.bufferViews[accessor.bufferView];

				CreateVulkanBufferAndMemory(VulkanUploader, loadedModel.buffers[bufferView.buffer].data.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					model._Meshes[meshIndex]._Tangents, model._BuffersHandle[bufferView.buffer], 0, false);

				model._Meshes[meshIndex]._tangent_offset = bufferView.byteOffset + accessor.byteOffset;
				model._Meshes[meshIndex]._tangent_nb = accessor.count;

			}

			meshIndex++;
		}
	}

	return true;
}

bool VulkanHelper::CreateModelFromRawVertices(Uploader& VulkanUploader, 
	VolatileLoopArray<vec3>& pos, 
	VolatileLoopArray<vec2>& uv, 
	VolatileLoopArray<vec3>& normals, 
	VolatileLoopArray<vec4>& vertexColor, 
	VolatileLoopArray<uint32_t>& indices, Model& model)
{
	bool noError = true;
	VkResult result = VK_SUCCESS;

	//making the mesh
	{

		// 1. creating and uploading the position buffer
		StaticBufferHandle posBuffer;
		noError |= CreateStaticBufferHandle(VulkanUploader, posBuffer, sizeof(vec3) * pos.Nb(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
			| VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
		noError |= UploadStaticBufferHandle(VulkanUploader, posBuffer, (void*)*pos, sizeof(vec3) * pos.Nb());

		// 2. creating and uploading the _Indices buffer (same index buffer is used for all of the meshes, but with different offset)
		StaticBufferHandle indexBuffer;
		noError |= CreateStaticBufferHandle(VulkanUploader, indexBuffer, sizeof(uint32_t) * indices.Nb(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
			| VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
		noError |= UploadStaticBufferHandle(VulkanUploader, indexBuffer, (void*)*indices, sizeof(uint32_t) * indices.Nb());

		// 3. creating and uploading the _Uvs buffer
		StaticBufferHandle uvBuffer;

		
		noError |= CreateStaticBufferHandle(VulkanUploader, uvBuffer, sizeof(vec2) * uv.Nb(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
			| VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
		noError |= UploadStaticBufferHandle(VulkanUploader, uvBuffer, (void*)*uv, sizeof(vec2) * uv.Nb());
		

		// 4. creating and uploading the normal buffer
		StaticBufferHandle normalBuffer;
		
		noError |= CreateStaticBufferHandle(VulkanUploader, normalBuffer, sizeof(vec3) * normals.Nb(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
			| VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
		noError |= UploadStaticBufferHandle(VulkanUploader, normalBuffer, (void*)*normals, sizeof(vec3) * normals.Nb());
		

		model._Meshes.Alloc(1);
		model._Meshes[0]._VertexMemoryHandle.Alloc(4);
		model._Meshes[0]._Positions				= posBuffer._StaticGPUBuffer;
		model._Meshes[0]._VertexMemoryHandle[0]	= posBuffer._StaticGPUMemoryHandle;
		model._Meshes[0]._Indices				= indexBuffer._StaticGPUBuffer;
		model._Meshes[0]._VertexMemoryHandle[1]	= indexBuffer._StaticGPUMemoryHandle;
		model._Meshes[0]._Uvs					= uvBuffer._StaticGPUBuffer;
		model._Meshes[0]._VertexMemoryHandle[2]	= uvBuffer._StaticGPUMemoryHandle;
		model._Meshes[0]._Normals				= normalBuffer._StaticGPUBuffer;
		model._Meshes[0]._VertexMemoryHandle[3]	= normalBuffer._StaticGPUMemoryHandle;

		model._Meshes[0]._pos_offset = 0;
		model._Meshes[0]._indices_offset = 0;
		model._Meshes[0]._normal_offset = 0;
		model._Meshes[0]._uv_offset		= 0;

		model._Meshes[0]._pos_nb		= pos.Nb();
		model._Meshes[0]._indices_nb	= indices.Nb();
		model._Meshes[0]._normal_nb		= normals.Nb();
		model._Meshes[0]._uv_nb			= uv.Nb();
		model._Meshes[0]._indices_type	= VK_INDEX_TYPE_UINT32;
	}

	//make texture
	{
		model._Textures.Alloc(1);
		noError |= LoadTexture(VulkanUploader, (void*)*vertexColor, vertexColor.Nb(), 1, VK_FORMAT_R32G32B32A32_SFLOAT, model._Textures[0]);
	}

	//make sampler
	{
		model._Samplers.Alloc(1);
		// a very basic sampler will be fine
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		VK_CALL_PRINT(vkCreateSampler(VulkanUploader._VulkanDevice, &samplerInfo, nullptr, &model._Samplers[0]));
	}

	//make material
	{
		model._Materials.Alloc(1);
		
		model._Materials[0]._Textures.Alloc(1);
		model._Textures[0]._Sampler = model._Samplers[0];
		model._Materials[0]._Textures[0] = model._Textures[0];
	}

	return result == VK_SUCCESS && noError;
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

void VulkanHelper::ImageMemoryBarrier(VkCommandBuffer cmdBuffer, const VkImage& image, VkImageLayout dstImageLayout, VkAccessFlagBits dstAccessMask, VkPipelineStageFlagBits syncScopeStart, VkPipelineStageFlagBits syncScopeEnd,
	VkAccessFlagBits srcAccessMask, VkImageLayout srcImageLayout, VkImageSubresourceRange imageRange)
{
	// putting a barrier on this image
	VkImageMemoryBarrier barrier{};
	barrier.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout			= srcImageLayout;//from nothing
	barrier.newLayout			= dstImageLayout;//to transfer dest
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.image				= image;//the image to put a barrier on
	barrier.subresourceRange	= imageRange;//what part of the image
	barrier.srcAccessMask		= srcAccessMask;// the previous access to the memory
	barrier.dstAccessMask		= dstAccessMask;// the wanted access to the memory

	//put the barrier on the scope wanted
	vkCmdPipelineBarrier(cmdBuffer, syncScopeStart, syncScopeEnd, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool VulkanHelper::CreateVulkanImageMemory(Uploader& VulkanUploader, VkMemoryPropertyFlags properties, const VkImage& image, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags flags)
{
	//getting the necessary requirements to create our image
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(VulkanUploader._VulkanDevice, image, &memRequirements);
	//then allocating the necessarry space
	return AllocateVulkanMemory(VulkanUploader, properties, memRequirements, GetMemoryTypeFromRequirements(properties, memRequirements, VulkanUploader._MemoryProperties), bufferMemory, flags);
}

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

	//allocates space if necessarry
	if (allocate_memory)
		CreateVulkanImageMemory(VulkanUploader, memoryProperties, imageToMake, imageMemory);

	VK_CALL_PRINT(vkBindImageMemory(VulkanUploader._VulkanDevice, imageToMake, imageMemory, offset))

	return result == VK_SUCCESS;
}

bool VulkanHelper::UploadImage(Uploader& VulkanUploader, VkImage& imageToUploadTo, void* image_content, uint32_t width, uint32_t height, VkFormat format, uint32_t depth)
{
	VkResult result = VK_SUCCESS;
	uint32_t size = width * height * depth * vkuGetFormatInfo(format).block_size;
	void* CPUHandle;

	// create a staging buffer that can hold the images data
	VkBuffer tempBuffer;
	VkDeviceMemory tempMemory;
	CreateVulkanBufferAndMemory(VulkanUploader, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tempBuffer, tempMemory);

	// first copy the data in the CPU memory allocated by Vulkan for transfer
	VK_CALL_PRINT(vkMapMemory(VulkanUploader._VulkanDevice, tempMemory, 0, size, 0, &CPUHandle));
	memcpy(CPUHandle, image_content, size);
	vkUnmapMemory(VulkanUploader._VulkanDevice, tempMemory);

	//make the image transferable to, for the transfer stage 
	ImageMemoryBarrier(VulkanUploader._CopyBuffer, imageToUploadTo, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, 
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

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

bool VulkanHelper::LoadTexture(Uploader& VulkanUploader, const void* pixels, uint32_t width, uint32_t height, VkFormat imageFormat, Texture& texture)
{
	VkResult result = VK_SUCCESS;

	if (pixels == nullptr)
		return false;

	//creating image from stbi info
	if (!CreateImage(VulkanUploader, texture._Image, texture._ImageMemory, width, height, 1u, VK_IMAGE_TYPE_2D, imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		return false;
	}

	//upload image to buffer
	if (!UploadImage(VulkanUploader, texture._Image, (void*)pixels, width, height, imageFormat))
	{
		return false;
	}

	//add a memory barrier, that allows for the texture to be read from a shader in the fragment shader
	ImageMemoryBarrier(VulkanUploader._CopyBuffer, texture._Image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	//create an image view associated with the created image to make it available in shader
	VkImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.sType						= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.subresourceRange.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.format						= imageFormat;
	viewCreateInfo.image						= texture._Image;
	viewCreateInfo.subresourceRange.layerCount	= 1;
	viewCreateInfo.subresourceRange.levelCount	= 1;
	viewCreateInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;

	VK_CALL_PRINT(vkCreateImageView(VulkanUploader._VulkanDevice, &viewCreateInfo, nullptr, &texture._ImageView));

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


/* Descriptors */

bool VulkanHelper::CreatePipelineDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, const MultipleVolatileMemory<VkDescriptorSetLayoutBinding>& Bindings, uint32_t bindingNb)
{
	//if an error happens...
	VkResult result = VK_SUCCESS;

	//copy the Bindings info inside the descriptor
	PipelineDescriptor._DescriptorBindings.Alloc(bindingNb);
	memcpy(*PipelineDescriptor._DescriptorBindings, *Bindings, sizeof(VkDescriptorSetLayoutBinding) * bindingNb);

	//creating the descriptor layout from the info given
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = PipelineDescriptor._DescriptorBindings.Nb();
	layoutInfo.pBindings	= *PipelineDescriptor._DescriptorBindings;

	VK_CALL_PRINT(vkCreateDescriptorSetLayout(VulkanUploader._VulkanDevice, &layoutInfo, nullptr, &PipelineDescriptor._DescriptorLayout));

	return result == VK_SUCCESS;
}

bool VulkanHelper::AllocateDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, uint32_t DescriptorSetNb)
{
	//if an error happens...
	VkResult result = VK_SUCCESS;

	//creating our pools
	{
		uint32_t descriptorNb = PipelineDescriptor._DescriptorBindings.Nb();

		MultipleScopedMemory<VkDescriptorPoolSize> PoolInfo;
		PoolInfo.Alloc(descriptorNb);

		for (uint32_t i = 0; i < PipelineDescriptor._DescriptorBindings.Nb(); i++)
		{
			PoolInfo[i].type			= PipelineDescriptor._DescriptorBindings[i].descriptorType;
			PoolInfo[i].descriptorCount = PipelineDescriptor._DescriptorBindings[i].descriptorCount * DescriptorSetNb;

		}

		//creating our descriptor pool to allocate the number of sets user asked for
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= descriptorNb;
		poolInfo.pPoolSizes		= *PoolInfo;
		poolInfo.maxSets		= DescriptorSetNb;

		VK_CALL_PRINT(vkCreateDescriptorPool(VulkanUploader._VulkanDevice, &poolInfo, nullptr, &PipelineDescriptor._DescriptorPool))
	}

	//allocating our sets
	{
		MultipleScopedMemory<VkDescriptorSetLayout> layouts{ DescriptorSetNb };
		for (uint32_t i = 0; i < DescriptorSetNb; i++)
			memcpy(&layouts[i], &PipelineDescriptor._DescriptorLayout, sizeof(VkDescriptorSetLayout));
		
		//allocating the number of sets user asked for
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= PipelineDescriptor._DescriptorPool;
		allocInfo.descriptorSetCount	= DescriptorSetNb;
		allocInfo.pSetLayouts			= *layouts;

		//then create the resource
		PipelineDescriptor._DescriptorSets.Alloc(DescriptorSetNb);
		VK_CALL_PRINT(vkAllocateDescriptorSets(VulkanUploader._VulkanDevice, &allocInfo, *PipelineDescriptor._DescriptorSets));
	}

	return result == VK_SUCCESS;
}

void VulkanHelper::ReleaseDescriptor(const VkDevice& VulkanDevice, PipelineDescriptors& PipelineDescriptor)
{
	if (PipelineDescriptor._DescriptorPool)
		vkDestroyDescriptorPool(VulkanDevice,PipelineDescriptor._DescriptorPool,nullptr);
	PipelineDescriptor._DescriptorPool = VK_NULL_HANDLE;
	PipelineDescriptor._DescriptorSets.Clear();
}

void VulkanHelper::UploadDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, const VkAccelerationStructureKHR& AS, uint32_t descriptorBindingIndex, uint32_t descriptorSetIndex)
{
	//the acceleration structure data
	VkWriteDescriptorSetAccelerationStructureKHR ASInfo{};
	ASInfo.sType						= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	ASInfo.pAccelerationStructures		= &AS;
	ASInfo.accelerationStructureCount	= 1;

	//finding the real index from the binding
	uint32_t index = 0;
	for (; index < PipelineDescriptor._DescriptorBindings.Nb(); index++)
		if (PipelineDescriptor._DescriptorBindings[index].binding == descriptorBindingIndex)
			break;

	//the binding does not exist in this pipeline descriptor, we quit
	if (index == PipelineDescriptor._DescriptorBindings.Nb())
		return;

	//writing to the set
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstBinding		= PipelineDescriptor._DescriptorBindings[index].binding;
	descriptorWrite.dstSet			= PipelineDescriptor._DescriptorSets[descriptorSetIndex];
	descriptorWrite.dstArrayElement	= 0;
	descriptorWrite.descriptorType	= PipelineDescriptor._DescriptorBindings[index].descriptorType;
	descriptorWrite.descriptorCount	= PipelineDescriptor._DescriptorBindings[index].descriptorCount;
	descriptorWrite.pNext			= &ASInfo;
	vkUpdateDescriptorSets(VulkanUploader._VulkanDevice, 1, &descriptorWrite, 0, nullptr);
}


void VulkanHelper::UploadDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, const VkBuffer& Buffer, uint32_t offset, uint32_t range, uint32_t descriptorBindingIndex, uint32_t descriptorSetIndex)
{
	//the buffer data
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer	= Buffer;
	bufferInfo.offset	= offset;
	bufferInfo.range	= range;

	//finding the real index from the binding
	uint32_t index = 0;
	for (; index < PipelineDescriptor._DescriptorBindings.Nb(); index++)
		if (PipelineDescriptor._DescriptorBindings[index].binding == descriptorBindingIndex)
			break;

	//the binding does not exist in this pipeline descriptor, we quit
	if (index == PipelineDescriptor._DescriptorBindings.Nb())
		return;

	//writing to the set
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstBinding			= PipelineDescriptor._DescriptorBindings[index].binding;
	descriptorWrite.dstSet				= PipelineDescriptor._DescriptorSets[descriptorSetIndex];
	descriptorWrite.dstArrayElement		= 0;
	descriptorWrite.descriptorType		= PipelineDescriptor._DescriptorBindings[index].descriptorType;
	descriptorWrite.descriptorCount		= PipelineDescriptor._DescriptorBindings[index].descriptorCount;
	descriptorWrite.pBufferInfo			= &bufferInfo;
	vkUpdateDescriptorSets(VulkanUploader._VulkanDevice, 1, &descriptorWrite, 0, nullptr);
}

void VulkanHelper::UploadDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, const VkImageView& ImageView, const VkSampler& Sampler, VkImageLayout ImageLayout, uint32_t descriptorBindingIndex, uint32_t descriptorSetIndex)
{
	//the image data
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageView		= ImageView;
	imageInfo.sampler		= Sampler;
	imageInfo.imageLayout	= ImageLayout;

	//finding the real index from the binding
	uint32_t index = 0;
	for (; index < PipelineDescriptor._DescriptorBindings.Nb(); index++)
		if (PipelineDescriptor._DescriptorBindings[index].binding == descriptorBindingIndex)
			break;

	//the binding does not exist in this pipeline descriptor, we quit
	if (index == PipelineDescriptor._DescriptorBindings.Nb())
		return;

	//writing to the set
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstBinding		= PipelineDescriptor._DescriptorBindings[index].binding;
	descriptorWrite.dstSet			= PipelineDescriptor._DescriptorSets[descriptorSetIndex];
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType	= PipelineDescriptor._DescriptorBindings[index].descriptorType;
	descriptorWrite.descriptorCount = PipelineDescriptor._DescriptorBindings[index].descriptorCount;
	descriptorWrite.pImageInfo		= &imageInfo;
	vkUpdateDescriptorSets(VulkanUploader._VulkanDevice, 1, &descriptorWrite, 0, nullptr);
}

void VulkanHelper::ClearPipelineDescriptor(const VkDevice& VulkanDevice, PipelineDescriptors& PipelineDescriptor)
{
	ReleaseDescriptor(VulkanDevice, PipelineDescriptor);
	if (PipelineDescriptor._DescriptorLayout)
		vkDestroyDescriptorSetLayout(VulkanDevice, PipelineDescriptor._DescriptorLayout, nullptr);
	PipelineDescriptor._DescriptorLayout = VK_NULL_HANDLE;
	PipelineDescriptor._DescriptorBindings.Clear();
}

/* FrameBuffer */

bool VulkanHelper::CreatePipelineOutput(Uploader& VulkanUploader, PipelineOutput& PipelineOutput, const MultipleVolatileMemory<VkAttachmentDescription>& colorAttachements, uint32_t colorAttachementNb, const VkAttachmentDescription& depthStencilAttachement)
{
	//if an error happens...
	VkResult result = VK_SUCCESS;

	//copy the attachement info inside the struct
	PipelineOutput._OutputColorAttachement.Alloc(colorAttachementNb);
	memcpy(*PipelineOutput._OutputColorAttachement, *colorAttachements, sizeof(VkAttachmentDescription) * colorAttachementNb);

	PipelineOutput._OutputDepthStencilAttachement = depthStencilAttachement;

	//we'll consider the depth attachement only if the format is a proper depth format
	bool usesDepth = PipelineOutput._OutputDepthStencilAttachement.format >= VK_FORMAT_D16_UNORM && PipelineOutput._OutputDepthStencilAttachement.format <= VK_FORMAT_D32_SFLOAT_S8_UINT;

	{
		//first create all the attachement necessary
		MultipleScopedMemory<VkAttachmentReference> colorAttachementRef{ colorAttachementNb};

		//there is a lot of possible attachement, but for our use case in this application, this makes sense
		for (uint32_t i = 0; i < colorAttachementNb; i++)
		{
			colorAttachementRef[i].attachment = i;
			colorAttachementRef[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}

		//describing the subpass of the renderpass
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount	= colorAttachementNb;
		subpass.pColorAttachments		= *colorAttachementRef;

		if (usesDepth)
		{
			//describing the depth stencil output in the subpass
			VkAttachmentReference depthBufferAttachmentRef{};
			depthBufferAttachmentRef.attachment = colorAttachementNb;
			depthBufferAttachmentRef.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			subpass.pDepthStencilAttachment = &depthBufferAttachmentRef;
		}

		//describing the renderpass
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType		= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses	= &subpass;

		if (usesDepth)
		{
			PipelineOutput._OutputClearValue.Alloc(colorAttachementNb + 1);
			ZERO_SET(PipelineOutput._OutputClearValue, sizeof(VkClearValue) * PipelineOutput._OutputClearValue.Nb());

			//copy the color attachement info
			MultipleScopedMemory<VkAttachmentDescription> colorAndDepthAttachement{ colorAttachementNb + 1 };
			memcpy(*colorAndDepthAttachement, *colorAttachements, sizeof(VkAttachmentDescription) * colorAttachementNb);
			//then the depth
			colorAndDepthAttachement[colorAttachementNb] = depthStencilAttachement;

			//put our new array with all the attachements
			renderPassInfo.pAttachments		= *colorAndDepthAttachement;
			renderPassInfo.attachmentCount	= colorAttachementNb + 1;

			//then create the render pass
			VK_CALL_PRINT(vkCreateRenderPass(VulkanUploader._VulkanDevice, &renderPassInfo, nullptr, &PipelineOutput._OutputRenderPass));
		}
		else
		{
			PipelineOutput._OutputClearValue.Alloc(colorAttachementNb);
			ZERO_SET(PipelineOutput._OutputClearValue, sizeof(VkClearValue) * PipelineOutput._OutputClearValue.Nb());

			//only color attachement are required ...
			renderPassInfo.pAttachments		= *PipelineOutput._OutputColorAttachement;
			renderPassInfo.attachmentCount	= colorAttachementNb;

			//... create the render pass with the info as-is
			VK_CALL_PRINT(vkCreateRenderPass(VulkanUploader._VulkanDevice, &renderPassInfo, nullptr, &PipelineOutput._OutputRenderPass));
		}
	}

	return result == VK_SUCCESS;
}

void VulkanHelper::AllocateFrameBuffer(PipelineOutput& PipelineOutput, uint32_t width, uint32_t height, uint32_t framebufferNb)
{
	//filling up the viewport and scissors
	PipelineOutput._OutputViewport.width	= static_cast<float>(width);
	PipelineOutput._OutputViewport.height	= static_cast<float>(height);
	PipelineOutput._OutputViewport.maxDepth = 1.0f;
	PipelineOutput._OutputViewport.minDepth = 0.0f;
	PipelineOutput._OutputViewport.x		= 0.0f;
	PipelineOutput._OutputViewport.y		= 0.0f;
	PipelineOutput._OutputScissor.extent	= { width, height };
	PipelineOutput._OutputScissor.offset	= { 0, 0};

	PipelineOutput._OuputFrameBuffer.Alloc(framebufferNb);
}

void VulkanHelper::SetClearValue(PipelineOutput& PipelineOutput, const VkClearValue& clearValue, uint32_t clearValueIndex)
{
	PipelineOutput._OutputClearValue[clearValueIndex] = clearValue;
}

bool VulkanHelper::SetFrameBuffer(Uploader& VulkanUploader, PipelineOutput& PipelineOutput, const MultipleVolatileMemory<VkImageView>& framebuffers, uint32_t frameBufferIndex)
{
	//if an error happens...
	VkResult result = VK_SUCCESS;

	bool usesDepth = PipelineOutput._OutputDepthStencilAttachement.format >= VK_FORMAT_D16_UNORM && PipelineOutput._OutputDepthStencilAttachement.format <= VK_FORMAT_D32_SFLOAT_S8_UINT;

	//describing the framebuffer
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass		= PipelineOutput._OutputRenderPass;
	framebufferInfo.width			= PipelineOutput._OutputScissor.extent.width;
	framebufferInfo.height			= PipelineOutput._OutputScissor.extent.height;
	framebufferInfo.layers			= 1;
	framebufferInfo.attachmentCount = PipelineOutput._OutputColorAttachement.Nb() + static_cast<uint32_t>(usesDepth);//we add the depth at the end
	framebufferInfo.pAttachments	= *framebuffers;//this expects the depth output to be at the end

	//making our framebuffer
	VK_CALL_PRINT(vkCreateFramebuffer(VulkanUploader._VulkanDevice, &framebufferInfo, nullptr, &PipelineOutput._OuputFrameBuffer[frameBufferIndex]))

	return result == VK_SUCCESS;
}

void VulkanHelper::ReleaseFrameBuffer(const VkDevice& VulkanDevice, PipelineOutput& PipelineOutput)
{
	VK_CLEAR_ARRAY(PipelineOutput._OuputFrameBuffer, PipelineOutput._OuputFrameBuffer.Nb(), vkDestroyFramebuffer, VulkanDevice);
}

void VulkanHelper::ClearPipelineOutput(const VkDevice& VulkanDevice, PipelineOutput& PipelineOutput)
{
	ReleaseFrameBuffer(VulkanDevice, PipelineOutput);
	if (PipelineOutput._OutputRenderPass)
		vkDestroyRenderPass(VulkanDevice, PipelineOutput._OutputRenderPass, nullptr);
	PipelineOutput._OutputRenderPass = VK_NULL_HANDLE;
	PipelineOutput._OutputColorAttachement.Clear();
	PipelineOutput._OutputClearValue.Clear();
}



bool VulkanHelper::CreateFrameBuffer(Uploader& VulkanUploader, FrameBuffer& Framebuffer, const PipelineOutput& Output, uint32_t associatedAttechementIndex, VkFormat overrideFormat)
{
	//if an error happens...
	VkResult result = VK_SUCCESS;

	const VkAttachmentDescription& associatedAttachement = Output._OutputColorAttachement[associatedAttechementIndex];

	//allocate the same number of images and render target than there is framebuffers
	uint32_t framebufferNb = Output._OuputFrameBuffer.Nb();
	Framebuffer._Images.Alloc(framebufferNb);
	Framebuffer._ImageViews.Alloc(framebufferNb);
	
	// creating the template image object with data given in parameters
	VkImageCreateInfo imageInfo{};
	imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType		= VK_IMAGE_TYPE_2D;//this helper is only for 2D images
	imageInfo.format		= overrideFormat != VK_FORMAT_UNDEFINED ? overrideFormat : associatedAttachement.format;//choose a format between both
	imageInfo.extent.width	= Output._OutputScissor.extent.width;//we get width and height from the scissors 
	imageInfo.extent.height = Output._OutputScissor.extent.height;// (we may want to change the size to save memory, but for now this is easier)
	imageInfo.extent.depth	= 1;
	imageInfo.mipLevels		= 1u;
	imageInfo.arrayLayers	= 1u;
	imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL;//will only load images from buffers, so no need to change this
	imageInfo.initialLayout = associatedAttachement.initialLayout;//initial layout will be the same as attachement
	imageInfo.usage			= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//in the definition of framebuffer in this helper, we need this
	imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;//we have two command queues : one for the app, one for the UI. the UI shouldn't use iamge we create.
	imageInfo.samples		= associatedAttachement.samples;//getting from attachement (but should be always one)
	imageInfo.flags			= 0; // Optional

	// we have the same memory for all our images, this is the size (or offset) needed to go from one to another
	VkDeviceSize imageOffset = 0;

	//allocating the memory on the GPU
	{

		VK_CALL_PRINT(vkCreateImage(VulkanUploader._VulkanDevice, &imageInfo, nullptr, &Framebuffer._Images[0]));

		//getting the necessary requirements to create our image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(VulkanUploader._VulkanDevice, Framebuffer._Images[0], &memRequirements);
		imageOffset = AlignUp(memRequirements.size, memRequirements.alignment);

		//trying to find a matching memory type between what the app wants and the device's limitation.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize	= imageOffset * framebufferNb;//we want multiple frames
		allocInfo.memoryTypeIndex	= VulkanHelper::GetMemoryTypeFromRequirements(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements, VulkanUploader._MemoryProperties);

		//allocating and associate the memory to our image.
		VK_CALL_PRINT(vkAllocateMemory(VulkanUploader._VulkanDevice, &allocInfo, nullptr, &Framebuffer._ImagesMemory));

		vkDestroyImage(VulkanUploader._VulkanDevice, Framebuffer._Images[0], nullptr);
	}

	for (uint32_t i = 0; i < framebufferNb; i++)
	{
		//create the image object from the template ...
		VK_CALL_PRINT(vkCreateImage(VulkanUploader._VulkanDevice, &imageInfo, nullptr, &Framebuffer._Images[i]));
		//then associate with the allocated memory
		VK_CALL_PRINT(vkBindImageMemory(VulkanUploader._VulkanDevice, Framebuffer._Images[i], Framebuffer._ImagesMemory, imageOffset * i))

		//make it immediately available to bind to shader
		ImageMemoryBarrier(VulkanUploader._CopyBuffer, Framebuffer._Images[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_NONE, associatedAttachement.initialLayout);

		//create an image view associated with the GPU local Image to make it available in shader
		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;//framebuffer is only for colorbuffer for now...
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;//framebuffer only supports 2D texture for now...
		viewCreateInfo.format = imageInfo.format;//get format from attachement info
		viewCreateInfo.image = Framebuffer._Images[i];
		viewCreateInfo.subresourceRange.layerCount = 1;
		viewCreateInfo.subresourceRange.levelCount = 1;

		VK_CALL_PRINT(vkCreateImageView(VulkanUploader._VulkanDevice, &viewCreateInfo, nullptr, &Framebuffer._ImageViews[i]));
	}

	return result == VK_SUCCESS;
}

bool VulkanHelper::CreateFrameBuffer(Uploader& VulkanUploader, FrameBuffer& Framebuffer, const VkImageCreateInfo& imageInfo, uint32_t frameBufferNb)
{
	//if an error happens...
	VkResult result = VK_SUCCESS;


	//allocate the same number of images and render target than there is framebuffers
	Framebuffer._Images.Alloc(frameBufferNb);
	Framebuffer._ImageViews.Alloc(frameBufferNb);

	// we have the same memory for all our images, this is the size (or offset) needed to go from one to another
	VkDeviceSize imageOffset = 0;

	//allocating the memory on the GPU
	{
		VK_CALL_PRINT(vkCreateImage(VulkanUploader._VulkanDevice, &imageInfo, nullptr, &Framebuffer._Images[0]));

		//getting the necessary requirements to create our image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(VulkanUploader._VulkanDevice, Framebuffer._Images[0], &memRequirements);
		imageOffset = AlignUp(memRequirements.size, memRequirements.alignment);

		//trying to find a matching memory type between what the app wants and the device's limitation.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = imageOffset * frameBufferNb;//we want multiple frames
		allocInfo.memoryTypeIndex = VulkanHelper::GetMemoryTypeFromRequirements(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements, VulkanUploader._MemoryProperties);

		//allocating and associate the memory to our image.
		VK_CALL_PRINT(vkAllocateMemory(VulkanUploader._VulkanDevice, &allocInfo, nullptr, &Framebuffer._ImagesMemory));

		vkDestroyImage(VulkanUploader._VulkanDevice, Framebuffer._Images[0], nullptr);
	}

	for (uint32_t i = 0; i < frameBufferNb; i++)
	{
		//create the image object from the template ...
		VK_CALL_PRINT(vkCreateImage(VulkanUploader._VulkanDevice, &imageInfo, nullptr, &Framebuffer._Images[i]));
		//then associate with the allocated memory
		VK_CALL_PRINT(vkBindImageMemory(VulkanUploader._VulkanDevice, Framebuffer._Images[i], Framebuffer._ImagesMemory, imageOffset * i))

			//make it immediately available to bind to shader
			ImageMemoryBarrier(VulkanUploader._CopyBuffer, Framebuffer._Images[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_NONE, imageInfo.initialLayout);

		//create an image view associated with the GPU local Image to make it available in shader
		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.subresourceRange.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;//framebuffer is only for colorbuffer for now...
		viewCreateInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;//framebuffer only supports 2D texture for now...
		viewCreateInfo.format						= imageInfo.format;//get format from image info
		viewCreateInfo.image						= Framebuffer._Images[i];
		viewCreateInfo.subresourceRange.layerCount = 1;
		viewCreateInfo.subresourceRange.levelCount = 1;

		VK_CALL_PRINT(vkCreateImageView(VulkanUploader._VulkanDevice, &viewCreateInfo, nullptr, &Framebuffer._ImageViews[i]));
	}

	return result == VK_SUCCESS;
}

void VulkanHelper::ClearFrameBuffer(const VkDevice& VulkanDevice, FrameBuffer& Framebuffer)
{
	VK_CLEAR_ARRAY(Framebuffer._ImageViews, Framebuffer._Images.Nb(), vkDestroyImageView, VulkanDevice);
	VK_CLEAR_ARRAY(Framebuffer._Images, Framebuffer._Images.Nb(), vkDestroyImage, VulkanDevice);
	if (Framebuffer._ImagesMemory)
		vkFreeMemory(VulkanDevice, Framebuffer._ImagesMemory, nullptr);
	Framebuffer._ImagesMemory = VK_NULL_HANDLE;
}


/* Acceleration Structures */

bool VulkanHelper::CreateRaytracedGeometry(Uploader& VulkanUploader, const VkAccelerationStructureGeometryKHR& vkGeometry, const VkAccelerationStructureBuildRangeInfoKHR& vkBuildRangeInfo, RaytracedGeometry& raytracedGeometry, VkAccelerationStructureBuildGeometryInfoKHR& vkBuildInfo, uint32_t index, uint32_t customInstanceIndex, uint32_t shaderOffset)
{
	//filling the build struct if not already done
	vkBuildInfo.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	vkBuildInfo.type			= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;//we are specifying a geometry, this makes it a bottom level
	vkBuildInfo.pGeometries		= &vkGeometry;
	vkBuildInfo.geometryCount	= 1;
	vkBuildInfo.mode			= VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;//we are creating it for the fist (and propably last) time
	vkBuildInfo.flags			= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

	//the size the AS buffer will need to be considering the geometry we've referenced above
	VkAccelerationStructureBuildSizesInfoKHR buildSize{};
	buildSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkGetAccelerationStructureBuildSizesKHR, VulkanUploader._VulkanDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &vkBuildInfo, &vkBuildRangeInfo.primitiveCount, &buildSize);

	//creating the buffer and memory that will contain our acceleration structure
	VulkanHelper::CreateVulkanBufferAndMemory(VulkanUploader, buildSize.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		raytracedGeometry._AccelerationStructureBuffer[index], raytracedGeometry._AccelerationStructureMemory[index], 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

	//setting our newly created buffer to create our acceleration structure object
	VkAccelerationStructureCreateInfoKHR ASCreateInfo{};
	ASCreateInfo.sType	= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	ASCreateInfo.type	= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	ASCreateInfo.buffer = raytracedGeometry._AccelerationStructureBuffer[index];
	ASCreateInfo.size	= buildSize.accelerationStructureSize;
	//creating a new acceleration structure
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCreateAccelerationStructureKHR, VulkanUploader._VulkanDevice, &ASCreateInfo, nullptr, &raytracedGeometry._AccelerationStructure[index]);

	//set our new Acceleration Structure Object to be the one receiving the geometry we've specified
	vkBuildInfo.dstAccelerationStructure = raytracedGeometry._AccelerationStructure[index];

	VkBuffer tmpBuffer;
	VkDeviceMemory tmpMemory;
	CreateTmpBufferAndAddress(VulkanUploader, buildSize.buildScratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
																		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
																		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
																		tmpBuffer, tmpMemory, vkBuildInfo.scratchData.deviceAddress, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

	if (raytracedGeometry._CustomInstanceIndex != nullptr)
		raytracedGeometry._CustomInstanceIndex[index] = customInstanceIndex;

	if (raytracedGeometry._ShaderOffset != nullptr)
		raytracedGeometry._ShaderOffset[index] = shaderOffset;

	return true;
}


//creating a bottom level AS for each mesh of model
bool VulkanHelper::CreateRaytracedGeometryFromMesh(Uploader& VulkanUploader, RaytracedGeometry& raytracedGeometry, const VolatileLoopArray<Mesh>& mesh, const VkBuffer& transformBuffer, const MultipleVolatileMemory<uint32_t>& transformOffset, uint32_t customInstanceIndex, uint32_t shaderOffset)
{
	//if an error happens...
	VkResult result = VK_SUCCESS;

	//used to get back the GPU adress of buffers
	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;

	//used to build the Acceleration structure
	VkAccelerationStructureGeometryTrianglesDataKHR bottomLevelMeshASData{};
	bottomLevelMeshASData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

	//array for the actual geometry description (in our case triangles)
	MultipleScopedMemory<VkAccelerationStructureGeometryKHR>			bottomLevelMeshAS{ mesh.Nb() };
	memset(*bottomLevelMeshAS, 0, sizeof(VkAccelerationStructureGeometryKHR) * mesh.Nb());
	//array for the build requests (one for each mesh)
	MultipleScopedMemory<VkAccelerationStructureBuildGeometryInfoKHR> bottomLevelMeshASInfo{mesh.Nb() };
	memset(*bottomLevelMeshASInfo, 0, sizeof(VkAccelerationStructureBuildGeometryInfoKHR) * mesh.Nb());
	//the extent of the buffer of each meshes' geometry
	MultipleScopedMemory<VkAccelerationStructureBuildRangeInfoKHR>	bottomLevelMeshASRangeInfo{mesh.Nb() };
	memset(*bottomLevelMeshASRangeInfo, 0, sizeof(VkAccelerationStructureBuildRangeInfoKHR) * mesh.Nb());
	MultipleScopedMemory<VkAccelerationStructureBuildRangeInfoKHR*>	bottomLevelMeshASRangeInfoPtr{ mesh.Nb() };
	memset(*bottomLevelMeshASRangeInfo, 0, sizeof(VkAccelerationStructureBuildRangeInfoKHR) * mesh.Nb());

	//allocating buffer and memory beforehand, as there will be one acceleration structure for each mesh
	raytracedGeometry._AccelerationStructure.Alloc(mesh.Nb());
	raytracedGeometry._AccelerationStructureBuffer.Alloc(mesh.Nb());
	raytracedGeometry._AccelerationStructureMemory.Alloc(mesh.Nb());
	raytracedGeometry._CustomInstanceIndex.Alloc(mesh.Nb());
	raytracedGeometry._ShaderOffset.Alloc(mesh.Nb());

	bool usesOffset = transformOffset != nullptr;

	VkDeviceOrHostAddressConstKHR transformAddress = VkDeviceOrHostAddressConstKHR{ 0 };
	if (transformBuffer != VK_NULL_HANDLE)
	{
		addressInfo.buffer = transformBuffer;
		transformAddress = VkDeviceOrHostAddressConstKHR{ vkGetBufferDeviceAddress(VulkanUploader._VulkanDevice, &addressInfo) };
	}

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
		bottomLevelMeshASData.maxVertex		= iMesh._pos_nb;
		//we do not have the data to know "bottomLevelMeshAS.maxVertex"

		//getting back the indices' GPU address
		addressInfo.buffer = iMesh._Indices;
		GPUBufferAddress = vkGetBufferDeviceAddress(VulkanUploader._VulkanDevice, &addressInfo);

		//giving the indices
		bottomLevelMeshASData.indexData = VkDeviceOrHostAddressConstKHR{ GPUBufferAddress };// this is sure to be a static buffer as we will not change vertex data
		bottomLevelMeshASData.indexType = iMesh._indices_type;

		bottomLevelMeshASData.transformData = transformAddress;

		//we now have a proper triangle geometry
		VkAccelerationStructureGeometryKHR& meshAS = bottomLevelMeshAS[i];
		meshAS.sType		= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		meshAS.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		meshAS.flags		= VK_GEOMETRY_OPAQUE_BIT_KHR;
		meshAS.geometry.triangles = bottomLevelMeshASData;

		//setting the bounds of our mesh (as when loading from models, the buffer for multiple geometry may have been the same)
		VkAccelerationStructureBuildRangeInfoKHR& meshASRange = bottomLevelMeshASRangeInfo[i];
		meshASRange.firstVertex		= iMesh._pos_offset;
		meshASRange.primitiveOffset = iMesh._indices_offset;
		meshASRange.primitiveCount	= iMesh._indices_nb / 3;
		meshASRange.transformOffset = usesOffset ? transformOffset[i] : 0;
		bottomLevelMeshASRangeInfoPtr[i] = &meshASRange;

		CreateRaytracedGeometry(VulkanUploader, meshAS, meshASRange, raytracedGeometry, bottomLevelMeshASInfo[i], i, customInstanceIndex, shaderOffset);
	}

	//finally, asking to build all of the acceleration structures at once (this is basically a copy command, so it is a defered call)
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCmdBuildAccelerationStructuresKHR, VulkanUploader._CopyBuffer, mesh.Nb(), *bottomLevelMeshASInfo, *bottomLevelMeshASRangeInfoPtr);

	//making the AS accessible
	MemorySyncScope(VulkanUploader._CopyBuffer, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR, 
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);//basically just changing this memory to be visible, so the scope can be "immediate"

	return result == VK_SUCCESS;
}

bool VulkanHelper::CreateRaytracedProceduralFromAABB(Uploader& VulkanUploader, StaticBufferHandle& AABBStaticBuffer,  RaytracedGeometry& raytracedGeometry, const MultipleVolatileMemory<VkAabbPositionsKHR>& AABBs, uint32_t nb, uint32_t index, uint32_t customInstanceIndex, uint32_t shaderOffset)
{
	//firstly, creating the AABB GPU Buffer
	VulkanHelper::CreateStaticBufferHandle(VulkanUploader, AABBStaticBuffer, sizeof(VkAabbPositionsKHR) * nb, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
	VulkanHelper::UploadStaticBufferHandle(VulkanUploader, AABBStaticBuffer, *AABBs, sizeof(VkAabbPositionsKHR) * nb);

	//they will all use the same buffer so pre create the struct
	VkAccelerationStructureGeometryKHR aabb{};
	aabb.sType					= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	aabb.geometryType			= VK_GEOMETRY_TYPE_AABBS_KHR;
	aabb.flags					= VK_GEOMETRY_OPAQUE_BIT_KHR;
	aabb.geometry.aabbs.sType	= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
	aabb.geometry.aabbs.pNext	= nullptr;
	aabb.geometry.aabbs.stride	= sizeof(VkAabbPositionsKHR);

	//setting our newly created AABB buffer as data
	VK_GET_BUFFER_ADDRESS(VulkanUploader._VulkanDevice, VkDeviceOrHostAddressConstKHR, AABBStaticBuffer._StaticGPUBuffer, aabb.geometry.aabbs.data);

	//technically, we're building one at a time, so we only need one
	VkAccelerationStructureBuildRangeInfoKHR ASRange{};
	ASRange.firstVertex		= 0;
	ASRange.primitiveOffset = 0;
	ASRange.primitiveCount	= nb;
	ASRange.transformOffset = 0;
	
	//we'll let the function fill up our build struct
	VkAccelerationStructureBuildGeometryInfoKHR AABBBuildInfo{};
	CreateRaytracedGeometry(VulkanUploader, aabb, ASRange, raytracedGeometry, AABBBuildInfo, index, customInstanceIndex, shaderOffset);

	VkAccelerationStructureBuildRangeInfoKHR* rangePtr = &ASRange;
	//finally, asking to build the acceleration structures (this is basically a copy command, so it is a defered call)
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCmdBuildAccelerationStructuresKHR, VulkanUploader._CopyBuffer, 1, &AABBBuildInfo, &rangePtr);

	//making the AS accessible
	MemorySyncScope(VulkanUploader._CopyBuffer, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);//basically just changing this memory to be visible, so the scope can be "immediate"

	return true;
}


void VulkanHelper::ClearRaytracedGeometry(const VkDevice& VulkanDevice, RaytracedGeometry& raytracedGeometry)
{
	
	VK_CLEAR_KHR(raytracedGeometry._AccelerationStructure, raytracedGeometry._AccelerationStructure.Nb(), VulkanDevice, vkDestroyAccelerationStructureKHR);
	VK_CLEAR_ARRAY(raytracedGeometry._AccelerationStructureBuffer, raytracedGeometry._AccelerationStructureBuffer.Nb(), vkDestroyBuffer, VulkanDevice);
	VK_CLEAR_ARRAY(raytracedGeometry._AccelerationStructureMemory, raytracedGeometry._AccelerationStructureMemory.Nb(), vkFreeMemory, VulkanDevice);
	raytracedGeometry._CustomInstanceIndex.Clear();
	raytracedGeometry._ShaderOffset.Clear();
}

bool VulkanHelper::CreateInstanceFromGeometry(Uploader& VulkanUploader, RaytracedGroup& raytracedGroup, const mat4& transform, const MultipleVolatileMemory<RaytracedGeometry*>& geometry, uint32_t nb)
{
	//to record if an error happened
	VkResult result = VK_SUCCESS;

	VkAccelerationStructureBuildGeometryInfoKHR& instancesInfo = raytracedGroup._InstancesInfo;
	VkAccelerationStructureBuildRangeInfoKHR&	instancesRange = raytracedGroup._InstancesRange[0]; 

	//count the total nb of instance we need to create.
	uint32_t totalInstanceNb = 0;
	for (uint32_t i = 0; i < nb; i++)
		totalInstanceNb += geometry[i]->_AccelerationStructure.Nb();

	instancesRange.primitiveCount = totalInstanceNb;

	//the description of the Top level Acceleration Structure
	raytracedGroup._Instances.Alloc(totalInstanceNb);
	memset(*raytracedGroup._Instances, 0, sizeof(VkAccelerationStructureInstanceKHR) * totalInstanceNb);
	//the instances of TLAS
	MultipleVolatileMemory<VkAccelerationStructureGeometryKHR> instancesAS{ totalInstanceNb };
	memset(*instancesAS, 0, sizeof(VkAccelerationStructureGeometryKHR) * totalInstanceNb);

	//going through every BLAS in order to create the instance and associated TLAS
	{
		VkAccelerationStructureInstanceKHR templateInstance = {};//will be used as only some parameters change
		templateInstance.mask = 0xFF;//every instance will be hittable
		templateInstance.transform = { transform[0], transform[1], transform[2], transform[12],
								transform[4], transform[5], transform[6], transform[13],
								transform[8], transform[9], transform[10],  transform[14] };
		templateInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

		//all the instances make one group's "geometry"
		VkAccelerationStructureGeometryKHR templateGeom = {};
		templateGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		templateGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		templateGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		templateGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

		//creating the instance buffer where every instance
		VkDeviceAddress tmpAddress;
		VkDeviceSize	tmpSize = sizeof(VkAccelerationStructureInstanceKHR) * totalInstanceNb;
		CreateVulkanBufferAndMemory(VulkanUploader, tmpSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
															| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
															VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
															| VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			raytracedGroup._InstancesBufferHandle._StaticGPUBuffer, raytracedGroup._InstancesBufferHandle._StaticGPUMemoryHandle, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);
		VK_GET_BUFFER_ADDRESS(VulkanUploader._VulkanDevice, VkDeviceAddress, raytracedGroup._InstancesBufferHandle._StaticGPUBuffer, tmpAddress);

		uint32_t instanceIndex = 0;
		for (uint32_t i = 0; i < nb; i++)
		{
			//first, does it have specific shaderGroupIndices or custom Instance Indices
			bool useShaderGroup = geometry[i]->_ShaderOffset != nullptr;
			bool useCustomInstance = geometry[i]->_CustomInstanceIndex != nullptr;

			for (uint32_t j = 0; j < geometry[i]->_AccelerationStructureBuffer.Nb(); j++, instanceIndex++)
			{
				templateInstance.instanceCustomIndex = useCustomInstance ? geometry[i]->_CustomInstanceIndex[j] : 0;
				templateInstance.instanceShaderBindingTableRecordOffset = useShaderGroup ? geometry[i]->_ShaderOffset[j] : 0;

				raytracedGroup._Instances[instanceIndex] = templateInstance;
				VK_GET_BUFFER_ADDRESS(VulkanUploader._VulkanDevice, uint64_t, geometry[i]->_AccelerationStructureBuffer[j], raytracedGroup._Instances[instanceIndex].accelerationStructureReference);
				instancesAS[instanceIndex] = templateGeom;
				instancesAS[instanceIndex].geometry.instances.data = VkDeviceOrHostAddressConstKHR{ tmpAddress + instanceIndex * sizeof(VkAccelerationStructureInstanceKHR) };//we need to offset it from the buffer's beginning
			}
		}

		// copy our instances into the instances buffer
		void* tmpHandle;
		VK_CALL_PRINT(vkMapMemory(VulkanUploader._VulkanDevice, raytracedGroup._InstancesBufferHandle._StaticGPUMemoryHandle, 0, tmpSize, 0, &tmpHandle));
		memcpy(tmpHandle, *raytracedGroup._Instances, tmpSize);
		vkUnmapMemory(VulkanUploader._VulkanDevice, raytracedGroup._InstancesBufferHandle._StaticGPUMemoryHandle);
	}

	instancesInfo.sType			= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	instancesInfo.type			= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;//we are specifying an instance, so it is a top level
	instancesInfo.pGeometries	= *instancesAS;
	instancesInfo.geometryCount = 1;
	instancesInfo.flags			= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

	return result == VK_SUCCESS;
}

bool VulkanHelper::UpdateTransform(const VkDevice& VulkanDevice, RaytracedGroup& raytracedObject, const mat4& transform, uint32_t index, uint32_t nb)
{
	//to record if an error happened
	VkResult result = VK_SUCCESS;

	if (nb > raytracedObject._InstancesRange[0].primitiveCount || index + nb > raytracedObject._InstancesRange[0].primitiveCount)
	{
		printf("Update Transform of raytraced Object out of instances array's bound !");
		return false;
	}

	{
		VkTransformMatrixKHR mat = { transform[0], transform[1], transform[2], transform[12],
								transform[4], transform[5], transform[6], transform[13],
								transform[8], transform[9], transform[10],  transform[14] };

		for (uint32_t i = index; i < nb; i++)
		{
			raytracedObject._Instances[i].transform = mat;
		}

		void* tmpHandle;
		VK_CALL_PRINT(vkMapMemory(VulkanDevice, raytracedObject._InstancesBufferHandle._StaticGPUMemoryHandle, index * sizeof(VkAccelerationStructureInstanceKHR), nb * sizeof(VkAccelerationStructureInstanceKHR), 0, &tmpHandle));
		memcpy(tmpHandle, *raytracedObject._Instances, sizeof(VkAccelerationStructureInstanceKHR) * nb);
		vkUnmapMemory(VulkanDevice, raytracedObject._InstancesBufferHandle._StaticGPUMemoryHandle);
	}

	return result == VK_SUCCESS;
}

bool VulkanHelper::CreateRaytracedGroupFromGeometry(Uploader& VulkanUploader, RaytracedGroup& raytracedObject, const mat4& transform, const MultipleVolatileMemory<RaytracedGeometry*>& geometry, uint32_t nb)
{
	//to record if an error happened
	VkResult result = VK_SUCCESS;

	raytracedObject._InstancesRange.Alloc(1);
	VkAccelerationStructureBuildRangeInfoKHR& buildRange = raytracedObject._InstancesRange[0];
	buildRange = {};
	CreateInstanceFromGeometry(VulkanUploader, raytracedObject, transform, geometry, nb);
	raytracedObject._InstancesInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

	VkAccelerationStructureBuildSizesInfoKHR buildSize{};
	buildSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkGetAccelerationStructureBuildSizesKHR, VulkanUploader._VulkanDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &raytracedObject._InstancesInfo, &buildRange.primitiveCount, &buildSize);

	//we first need to create the Top Level AS Buffer
	{

		//creating our acceleration structure buffer
		VulkanHelper::CreateVulkanBufferAndMemory(VulkanUploader, buildSize.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			raytracedObject._AccelerationStructureBuffer, raytracedObject._AccelerationStructureMemory, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

		VkAccelerationStructureCreateInfoKHR instanceASCreateInfo{};
		instanceASCreateInfo.sType	= VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		instanceASCreateInfo.type	= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		instanceASCreateInfo.buffer = raytracedObject._AccelerationStructureBuffer;
		instanceASCreateInfo.size	= buildSize.accelerationStructureSize;

		VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCreateAccelerationStructureKHR, VulkanUploader._VulkanDevice, &instanceASCreateInfo, nullptr, &raytracedObject._AccelerationStructure);
	}

	raytracedObject._InstancesInfo.dstAccelerationStructure = raytracedObject._AccelerationStructure;

	{
		//we'll make the scratch buffer, by hand because we need to save the meomory type

		VkBuffer		scratchBuffer;
		VkDeviceMemory	scratchMemory;
		VkDeviceSize	scratchSize = buildSize.buildScratchSize;

		//making the buffer
		CreateVulkanBuffer(VulkanUploader, scratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, scratchBuffer);

		//first let's get the requirements that the memory should follow (like it needs to be 4 bytes and such)
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(VulkanUploader._VulkanDevice, scratchBuffer, &memRequirements);

		//this is what we need for future updates
		raytracedObject._ScratchMemoryType = GetMemoryTypeFromRequirements(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements, VulkanUploader._MemoryProperties);

		//then allocate
		AllocateVulkanMemory(VulkanUploader, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements, raytracedObject._ScratchMemoryType, scratchMemory, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);


		//then link together the buffer "interface" object, and the allcated memory
		VK_CALL_PRINT(vkBindBufferMemory(VulkanUploader._VulkanDevice, scratchBuffer, scratchMemory, 0));

		//get the address of our newly created buffer and give it to the addresss in parameters
		VK_GET_BUFFER_ADDRESS(VulkanUploader._VulkanDevice, VkDeviceAddress, scratchBuffer, raytracedObject._InstancesInfo.scratchData.deviceAddress);

		//do not forget to free the temporary buffer
		VulkanUploader._ToFreeBuffers.Add(scratchBuffer);
		VulkanUploader._ToFreeMemory.Add(scratchMemory);
	}

	VkAccelerationStructureBuildRangeInfoKHR* buildRangePtr = &buildRange;
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCmdBuildAccelerationStructuresKHR, VulkanUploader._CopyBuffer, 1, &raytracedObject._InstancesInfo, &buildRangePtr);

	//making the AS accessible
	MemorySyncScope(VulkanUploader._CopyBuffer, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);//basically just changing this memory to be visible, so the scope can be "immediate"

	return result == VK_SUCCESS;
}

bool VulkanHelper::UpdateRaytracedGroup(VulkanHelper::Uploader& VulkanUploader, RaytracedGroup& raytracedGroup)
{
	//to record if an error happened
	VkResult result = VK_SUCCESS;

	//we'll update
	raytracedGroup._InstancesInfo.srcAccelerationStructure = raytracedGroup._AccelerationStructure;
	raytracedGroup._InstancesInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;

	VkAccelerationStructureBuildSizesInfoKHR buildSize{};
	buildSize.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkGetAccelerationStructureBuildSizesKHR, VulkanUploader._VulkanDevice, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &raytracedGroup._InstancesInfo, &(*raytracedGroup._InstancesRange)->primitiveCount, &buildSize);

	{
		//we'll make the scratch buffer, by hand because we need to save the meomory type

		VkBuffer		scratchBuffer;
		VkDeviceMemory	scratchMemory;
		VkDeviceSize	scratchSize = buildSize.updateScratchSize;

		//making the buffer
		CreateVulkanBuffer(VulkanUploader, scratchSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, scratchBuffer);

		//first let's get the requirements that the memory should follow (like it needs to be 4 bytes and such)
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(VulkanUploader._VulkanDevice, scratchBuffer, &memRequirements);

		//then allocate
		AllocateVulkanMemory(VulkanUploader, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements, raytracedGroup._ScratchMemoryType, scratchMemory, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

		//then link together the buffer "interface" object, and the allcated memory
		VK_CALL_PRINT(vkBindBufferMemory(VulkanUploader._VulkanDevice, scratchBuffer, scratchMemory, 0));

		//get the address of our newly created buffer and give it to the addresss in parameters
		VK_GET_BUFFER_ADDRESS(VulkanUploader._VulkanDevice, VkDeviceAddress, scratchBuffer, raytracedGroup._InstancesInfo.scratchData.deviceAddress);

		//do not forget to free the temporary buffer
		VulkanUploader._ToFreeBuffers.Add(scratchBuffer);
		VulkanUploader._ToFreeMemory.Add(scratchMemory);
	}

	VK_CALL_KHR(VulkanUploader._VulkanDevice, vkCmdBuildAccelerationStructuresKHR, VulkanUploader._CopyBuffer, 1, &raytracedGroup._InstancesInfo, &raytracedGroup._InstancesRange);

	//making the AS accessible
	MemorySyncScope(VulkanUploader._CopyBuffer, VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);//basically just changing this memory to be visible, so the scope can be "immediate"

	return result == VK_SUCCESS;
}


void VulkanHelper::ClearRaytracedGroup(const VkDevice& VulkanDevice, RaytracedGroup& raytracedModel)
{
	//destroy acceleration structure data
	VK_CALL_KHR(VulkanDevice, vkDestroyAccelerationStructureKHR, VulkanDevice, raytracedModel._AccelerationStructure, nullptr);
	vkDestroyBuffer(VulkanDevice, raytracedModel._AccelerationStructureBuffer, nullptr);
	vkFreeMemory(VulkanDevice, raytracedModel._AccelerationStructureMemory, nullptr);

	//free instances data
	raytracedModel._Instances.Clear();
	ClearStaticBufferHandle(VulkanDevice, raytracedModel._InstancesBufferHandle);
	delete[] raytracedModel._InstancesInfo.pGeometries;
	raytracedModel._InstancesRange.Clear();
}


bool VulkanHelper::CreateSceneBufferFromMeshes(Uploader& VulkanUploader, SceneBuffer& sceneBuffer, const VolatileLoopArray<Mesh>& mesh)
{
	//to record if an error happened
	VkResult result = VK_SUCCESS;

	//very naive way of doing this. just copying the already allocated GPU memory into a new allocated scene buffer
	{
		//the offset buffer that tells the eqch mesh where its vertices are
		MultipleScopedMemory<uint32_t> offsetBuffer{mesh.Nb() * 3};
		memset(*offsetBuffer, 0, mesh.Nb() * sizeof(uint32_t) * 3);

		//to count the total nb of vertices there is to allocate a buffer of that size 
		uint32_t totalIndexNb = 0;
		uint32_t totalUVNb = 0;
		uint32_t totalNormalNb = 0;

		//this is a naive method and should be improved, but for now, hoping that no model with both short and long index will be used
		uint32_t indexSize = mesh[0]._indices_type == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t);

		//filling the offset buffer and the total count
		for (uint32_t i = 1; i <= mesh.Nb(); i++)
		{
			const Mesh& indexedMesh = mesh[i - 1];

			totalIndexNb	+= indexedMesh._indices_nb;
			totalUVNb		+= indexedMesh._uv_nb;
			totalNormalNb	+= indexedMesh._normal_nb;

			if (i == mesh.Nb())
				break;

			offsetBuffer[i * 3 + 0] = totalIndexNb;
			offsetBuffer[i * 3 + 1] = totalUVNb;
			offsetBuffer[i * 3 + 2] = totalNormalNb;
		}


		//we can determine the final size of each buffer
		sceneBuffer._IndexBufferSize	= totalIndexNb * indexSize;
		sceneBuffer._UVsBufferSize		= totalUVNb * sizeof(vec2);
		sceneBuffer._NormalBufferSize	= totalNormalNb * sizeof(vec3);

		//creating the buffer we'll fil with the vertices data 
		CreateStaticBufferHandle(VulkanUploader, sceneBuffer._IndexBuffer, sceneBuffer._IndexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		CreateStaticBufferHandle(VulkanUploader, sceneBuffer._UVsBuffer, sceneBuffer._UVsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		CreateStaticBufferHandle(VulkanUploader, sceneBuffer._NormalBuffer, sceneBuffer._NormalBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		//copy buffers
		for (uint32_t i = 0; i < mesh.Nb(); i++)
		{
			const Mesh& indexedMesh = mesh[i];

			VkBufferCopy region;
			region.srcOffset = indexedMesh._indices_offset;//this is already the offset in bytes for the indices
			region.dstOffset = offsetBuffer[i * 3] * indexSize;
			region.size		= indexedMesh._indices_nb * indexSize;
			vkCmdCopyBuffer(VulkanUploader._CopyBuffer, indexedMesh._Indices, sceneBuffer._IndexBuffer._StaticGPUBuffer, 1, &region);


			region.srcOffset = indexedMesh._uv_offset;//this is already the offset in bytes for the uvs
			region.dstOffset = offsetBuffer[i * 3 + 1] * sizeof(vec2);
			region.size = indexedMesh._uv_nb * sizeof(vec2);
			vkCmdCopyBuffer(VulkanUploader._CopyBuffer, indexedMesh._Uvs, sceneBuffer._UVsBuffer._StaticGPUBuffer, 1, &region);


			region.srcOffset = indexedMesh._normal_offset;//this is already the offset in bytes for the uvs
			region.dstOffset = offsetBuffer[i*3 + 2] * sizeof(vec3);
			region.size = indexedMesh._normal_nb * sizeof(vec3);
			vkCmdCopyBuffer(VulkanUploader._CopyBuffer, indexedMesh._Normals, sceneBuffer._NormalBuffer._StaticGPUBuffer, 1, &region);
		}

		//making the AS accessible
		MemorySyncScope(VulkanUploader._CopyBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);//basically just changing this memory to be visible, so the scope can be "immediate"


		sceneBuffer._OffsetBufferSize = mesh.Nb() * sizeof(uint32_t) * 3;

		//creating our offset buffer
		CreateStaticBufferHandle(VulkanUploader, sceneBuffer._OffsetBuffer, sceneBuffer._OffsetBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		UploadStaticBufferHandle(VulkanUploader, sceneBuffer._OffsetBuffer, *offsetBuffer, sceneBuffer._OffsetBufferSize);

	}

	return result == VK_SUCCESS;

}

bool VulkanHelper::CreateSceneBufferFromModels(Uploader& VulkanUploader, SceneBuffer& sceneBuffer, const MultipleVolatileMemory<Model>& models, uint32_t modelNb)
{
	//to record if an error happened
	VkResult result = VK_SUCCESS;

	uint32_t meshNb = 0;
	for (uint32_t i = 0; i < modelNb; i++)
		meshNb += models[i]._Meshes.Nb();

	//very naive way of doing this. just copying the already allocated GPU memory into a new allocated scene buffer
	{
		//the offset buffer that tells the eqch mesh where its vertices are
		MultipleScopedMemory<uint32_t> offsetBuffer{ meshNb * 3 };
		memset(*offsetBuffer, 0, meshNb * sizeof(uint32_t) * 3);

		//to count the total nb of vertices there is to allocate a buffer of that size 
		uint32_t totalIndexNb = 0;
		uint32_t totalUVNb = 0;
		uint32_t totalNormalNb = 0;

		//this is a naive method and should be improved, but for now, hoping that no model with both short and long index will be used
		uint32_t indexSize = models[0]._Meshes[0]._indices_type == VK_INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t);

		
		for (uint32_t j = 0, modelOffset = 0; j < modelNb; j++)
		{

			offsetBuffer[modelOffset + 0] = totalIndexNb;
			offsetBuffer[modelOffset + 1] = totalUVNb;
			offsetBuffer[modelOffset + 2] = totalNormalNb;

			//filling the offset buffer and the total count
			for (uint32_t i = 1; i <= models[j]._Meshes.Nb(); i++)
			{
				const Mesh& indexedMesh = models[j]._Meshes[i - 1];

				totalIndexNb += indexedMesh._indices_nb;
				totalUVNb += indexedMesh._uv_nb;
				totalNormalNb += indexedMesh._normal_nb;

				if (i == models[j]._Meshes.Nb())
					break;

				offsetBuffer[modelOffset + i * 3 + 0] = totalIndexNb;
				offsetBuffer[modelOffset + i * 3 + 1] = totalUVNb;
				offsetBuffer[modelOffset + i * 3 + 2] = totalNormalNb;
			}

			modelOffset = models[j]._Meshes.Nb() * 3;
		}

		//we can determine the final size of each buffer
		sceneBuffer._IndexBufferSize = totalIndexNb * indexSize;
		sceneBuffer._UVsBufferSize = totalUVNb * sizeof(vec2);
		sceneBuffer._NormalBufferSize = totalNormalNb * sizeof(vec3);

		//creating the buffer we'll fil with the vertices data 
		CreateStaticBufferHandle(VulkanUploader, sceneBuffer._IndexBuffer, sceneBuffer._IndexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		CreateStaticBufferHandle(VulkanUploader, sceneBuffer._UVsBuffer, sceneBuffer._UVsBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		CreateStaticBufferHandle(VulkanUploader, sceneBuffer._NormalBuffer, sceneBuffer._NormalBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

		//copy buffers
		for (uint32_t j = 0, modelOffset = 0; j < modelNb; j++)
		{
			for (uint32_t i = 0; i < models[j]._Meshes.Nb(); i++)
			{
				const Mesh& indexedMesh = models[j]._Meshes[i];

				VkBufferCopy region;
				region.srcOffset = indexedMesh._indices_offset;//this is already the offset in bytes for the indices
				region.dstOffset = offsetBuffer[modelOffset + i * 3] * indexSize;
				region.size = indexedMesh._indices_nb * indexSize;
				vkCmdCopyBuffer(VulkanUploader._CopyBuffer, indexedMesh._Indices, sceneBuffer._IndexBuffer._StaticGPUBuffer, 1, &region);


				region.srcOffset = indexedMesh._uv_offset;//this is already the offset in bytes for the uvs
				region.dstOffset = offsetBuffer[modelOffset + i * 3 + 1] * sizeof(vec2);
				region.size = indexedMesh._uv_nb * sizeof(vec2);
				vkCmdCopyBuffer(VulkanUploader._CopyBuffer, indexedMesh._Uvs, sceneBuffer._UVsBuffer._StaticGPUBuffer, 1, &region);


				region.srcOffset = indexedMesh._normal_offset;//this is already the offset in bytes for the uvs
				region.dstOffset = offsetBuffer[modelOffset + i * 3 + 2] * sizeof(vec3);
				region.size = indexedMesh._normal_nb * sizeof(vec3);
				vkCmdCopyBuffer(VulkanUploader._CopyBuffer, indexedMesh._Normals, sceneBuffer._NormalBuffer._StaticGPUBuffer, 1, &region);
			}
			
			modelOffset = models[j]._Meshes.Nb() * 3;
		}

		//making the AS accessible
		MemorySyncScope(VulkanUploader._CopyBuffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);//basically just changing this memory to be visible, so the scope can be "immediate"


		sceneBuffer._OffsetBufferSize = meshNb * sizeof(uint32_t) * 3;

		//creating our offset buffer
		CreateStaticBufferHandle(VulkanUploader, sceneBuffer._OffsetBuffer, sceneBuffer._OffsetBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		UploadStaticBufferHandle(VulkanUploader, sceneBuffer._OffsetBuffer, *offsetBuffer, sceneBuffer._OffsetBufferSize);

	}

	return result == VK_SUCCESS;
}


void VulkanHelper::ClearSceneBuffer(const VkDevice& VulkanDevice, SceneBuffer& sceneBuffer)
{
	ClearStaticBufferHandle(VulkanDevice, sceneBuffer._IndexBuffer);
	ClearStaticBufferHandle(VulkanDevice, sceneBuffer._NormalBuffer);
	ClearStaticBufferHandle(VulkanDevice, sceneBuffer._OffsetBuffer);
	ClearStaticBufferHandle(VulkanDevice, sceneBuffer._UVsBuffer);
}

