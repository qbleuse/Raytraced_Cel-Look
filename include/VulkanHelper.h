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
		delete [] raw_array;\
	}\

#define VK_CLEAR_ARRAY(_array, size, call, device) \
	if (_array != nullptr)\
	{\
		for (uint32_t i = 0; i < size; i++)\
			call(device, _array[i], nullptr);\
		_array.Clear();\
	}\

#define VK_CLEAR_LIST(_list, size, call, device) \
	if (_list.GetHead() != nullptr)\
	{\
		auto start = _list.GetHead();\
		for (uint32_t i = 0; i < size; i++)\
		{\
			call(device, start->data, nullptr);\
			start = ++(*start);\
		}\
		_list.Clear();\
	}\




/* forward def */
class GraphicsAPIManager;
struct GAPIHandle;

namespace VulkanHelper
{
	/* Uploader */

	struct Uploader
	{
		VkDevice			VulkanDevice;
		VkCommandBuffer		CopyBuffer;
		VkQueue				CopyQueue;
		VkCommandPool		CopyPool;

		List<VkBuffer>			ToFreeBuffers;
		List<VkDeviceMemory>	ToFreeMemory;

		VkPhysicalDeviceMemoryProperties	MemoryProperties;
	};

	// Creates an Uploader Object to use in all other Vulkan Helper method. will create an open command buffer for copy command and such
	bool StartUploader(class GraphicsAPIManager& GAPI, Uploader& VulkanUploader);

	// Submit the recorded copy commands, wait for it to finish, then releases the copy command memory.
	bool SubmitUploader(Uploader& VulkanUploader);

	/* Shaders */

	bool CreateVulkanShaders(Uploader& VulkanUploader, VkShaderModule& shader, VkShaderStageFlagBits shaderStage, const char* shaderSource, const char* shader_name, const char* entryPoint = "main");

	/* Buffers */

	uint32_t GetMemoryTypeFromRequirements(const VkMemoryPropertyFlags& wantedMemoryProperties, const VkMemoryRequirements& memoryRequirements, const VkPhysicalDeviceMemoryProperties& memoryProperties);

	/**
	* A struct in struct of array format used to create the necessary handles and pointers for a glsl shader's "uniform buffer", for each frame in flight.
	*/
	struct UniformBufferHandle
	{
		HeapMemory<VkBuffer>		GPUBuffer;
		HeapMemory<VkDeviceMemory>	GPUMemoryHandle;
		HeapMemory<void*>			CPUMemoryHandle;

		uint32_t nbBuffer;
	};

	/* Creates a one dimensionnal buffer of any usage and the association between CPU and GPU. if AllocateMemory is set to false, bufferMemory MUST BE VALID ! */
	bool CreateVulkanBuffer(Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, uint32_t offset = 0, bool AllocateMemory = true);


	/* Creates all the pointers and handle vulkan needs to create a buffer on the GPU and creates a UniformBufferHandle on the CPU to manage it*/
	bool CreateUniformBufferHandle(Uploader& VulkanUploader, UniformBufferHandle& bufferHandle, uint32_t bufferNb, VkDeviceSize size,
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, bool mapCPUMemoryHandle = true);

	/* Asks for de-allocation of all allocated resources for this UniformBufferHandle, on CPU and GPU*/
	void ClearUniformBufferHandle(const VkDevice& VulkanDevice, UniformBufferHandle& bufferHandle);

	/**
	* A struct representing a static and unique buffer on the GPU. You would use this type of buffer for data that will not change between frames such as vertex buffers.
	* include the staging buffer and the static buffer.
	*/
	struct StaticBufferHandle
	{
		VkBuffer		StaticGPUBuffer;
		VkDeviceMemory	StaticGPUMemoryHandle;
	};

	/* Creates all the pointers and handle vulkan needs to create a buffer on the GPU and creates a staging buffer to send the data */
	bool CreateStaticBufferHandle(Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, VkDeviceSize size,
		VkBufferUsageFlags staticBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VkMemoryPropertyFlags staticBufferProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	/* sends the data through the staging buffer to the static buffer */
	bool UploadStaticBufferHandle(Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, const void* data, VkDeviceSize size, bool releaseStagingBuffer = true);

	/* Asks for de-allocation of all allocated resources for this StaticBufferHandle on GPU */
	void ClearStaticBufferHandle(const VkDevice& VulkanDevice, StaticBufferHandle& bufferHandle);

	/* a struct representing a single mesh. this only represent vertex data meaning it contains :
	 * - positions
	 * - texture coordinates (uv)
	 * - normals
	 * - tangents
	 * - indices
	 * Only positions and indices are assumed to always be correct, other vertex data can be empty.
	 */
	struct Mesh
	{
		union
		{
			struct
			{
				VkBuffer positions;
				VkBuffer uvs;
				VkBuffer normals;
				VkBuffer tangents;
			};
			VkBuffer vertexBuffers[4];
		};

		union
		{
			struct
			{
				VkDeviceSize posOffset;
				VkDeviceSize uvOffset;
				VkDeviceSize normalOffset;
				VkDeviceSize tangentOffset;
			};
			VkDeviceSize vertexOffsets[4];
		};

		LoopArray<VkDeviceMemory> vertexMemoryHandle;

		VkBuffer	indices;
		uint32_t	indicesNb;
		uint32_t	indicesOffset;
		VkIndexType indicesType;

	};

	/* uses tiny obj to load to static buffers into Vulkan */
	void LoadObjFile(Uploader& VulkanUploader, const char* fileName, LoopArray<Mesh>& Meshes);

	/* Asks for de-allocation of all allocated resources for this mesh on GPU */
	void ClearMesh(const VkDevice& VulkanDevice, Mesh& mesh);

	/*
	* a struct representing a model. a model is composed of :
	* - a mesh with vertices data
	* - the associated texture information
	* - and material information
	*
	* /!\ material may not always be processed /!\
	*/
	struct Model
	{
		LoopArray<Mesh>				meshes;
		HeapMemory<uint32_t>		materialIndex;//same length as mesh

		LoopArray<VkDeviceMemory>	buffersHandle;

		LoopArray<struct Texture>	textures;
		LoopArray<VkSampler>		samplers;

		LoopArray<struct Material>	materials;
	};

	bool LoadGLTFFile(Uploader& VulkanUploader, const char* fileName, Model& Meshes);
	void ClearModel(const VkDevice& VulkanDevice, Model& model);

	/* Images */

	bool CreateImage(Uploader& VulkanUploader, VkImage& imageToMake, VkDeviceMemory& imageMemory, uint32_t width, uint32_t height, uint32_t depth, VkImageType imagetype, VkFormat format, VkImageUsageFlags usageFlags, VkMemoryPropertyFlagBits memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	bool UploadImage(Uploader& VulkanUploader, VkImage& imageToUploadTo, void* imageContent, uint32_t width, uint32_t height, uint32_t channels, uint32_t depth = 1);

	/*
	* a struct representing a single texture. it contains everything a shader would need to use said texture meaning :
	* - An Image
	* - An Image View
	* - A sampler (this is user's responsability)
	* Texture is considered a 2D image
	*/
	struct Texture
	{
		VkImage				image;
		VkDeviceMemory		imageMemory;
		VkImageView			imageView;
		VkSampler			sampler;
	};

	bool LoadTexture(Uploader& VulkanUploader, const char* fileName, Texture& texture);
	bool LoadTexture(Uploader& VulkanUploader, const void* pixels, uint32_t width, uint32_t height, VkFormat format, Texture& texture);

	void ClearTexture(const VkDevice& VulkanDevice, Texture& texture);


	/*
	* a struct representing an arbitrary material. It contains a texture and the associated descriptor and sampler.
	* Descriptors creation are of the responsability of the pipeline, as you need to have layout to actually use them.
	* You may not actually need to clear as textures and sampler may be the property of other 
	*/
	struct Material
	{
		LoopArray<Texture>			textures;

		VkDescriptorSet				textureDescriptors;
	};

};

#endif //__VULKAN_HELPER_H__