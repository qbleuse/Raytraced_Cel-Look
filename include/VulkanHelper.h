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

#define VK_CLEAR_RAW_ARRAY_NO_FREE(raw_array, size, call, device) \
	if (raw_array != nullptr)\
	{\
		for (uint32_t i = 0; i < size; i++)\
			if (raw_array[i] != nullptr)\
			{\
				call(device, raw_array[i], nullptr); \
				raw_array[i] = nullptr;\
			}\
	}\

#define VK_CLEAR_RAW_ARRAY(raw_array, size, call, device) \
	if (raw_array != nullptr)\
	{\
		for (uint32_t i = 0; i < size; i++)\
			if (raw_array[i] != nullptr)\
			{\
				call(device, raw_array[i], nullptr); \
				raw_array[i] = nullptr;\
			}\
		delete[] raw_array;\
	}\

#define VK_CLEAR_ARRAY(_array, size, call, device) \
	if (_array != nullptr)\
	{\
		for (uint32_t i = 0; i < size; i++)\
			if (_array[i] != nullptr)\
			{\
				call(device, _array[i], nullptr); \
				_array[i] = nullptr;\
			}\
		_array.Clear();\
	}\

#define VK_CLEAR_LIST(_list, size, call, device) \
	if (_list.Nb() > 0)\
	{\
		auto start = _list.GetHead();\
		for (uint32_t i = 0; i < size; i++)\
		{\
			call(device, start->data, nullptr);\
			start = ++(*start);\
		}\
		_list.Clear();\
	}\

#define VK_CALL_KHR(device, vk_call, ...) \
	{\
		auto func = (PFN_##vk_call) vkGetDeviceProcAddr(device, #vk_call);\
		if (func)\
		{\
			func(__VA_ARGS__);\
		}\
		else\
		{\
			printf("Vulkan extension call error : we could not find %s .\n", #vk_call); \
		}\
	}\

#define VK_GET_BUFFER_ADDRESS(device, type, var_buffer, var) \
	{\
		VkBufferDeviceAddressInfo addressInfo{};\
		addressInfo.sType	= VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;\
		addressInfo.buffer	= var_buffer;\
		var = type{ vkGetBufferDeviceAddress(device, &addressInfo) };\
	}\


/* forward def */
class GraphicsAPIManager;
struct GAPIHandle;
struct mat4;

namespace VulkanHelper
{
	/* Utilities */

	/*
	* Aligns a size or offset to the alignement given in parameter, making the offset a multiple of the alignement.
	*/
	inline uint32_t AlignUp(uint32_t size, uint32_t alignment) 
	{
		/*
		* Bitwise operation do still feel hisoteric even for me today so I'll explain what it does.
		* Tl;DR : if size < alignement it rounds up to alignement, otherwise it kind of removes the bits from alignement - 1 to make it a multiple.
		* 
		* here is an example : size = 3, aligment = 8
		* - (size + alignement - 1) = 3 + 8 - 1 = 10 ; in binary 0011 + 1000 - 0001 = 1010. This gives us a size that is over the original alignement.
		* - ~(alignement - 1) = NOT(1000 - 0001) = NOT(0111) = ...1000. with this all the bits of an uint_32t except the 3 Less Signicant Bit are ones
		* - 1010 & ...1000 = 1000. we get back "alignement" so we succefully got a size that is aligned.
		* 
		* another example : size = 10, alignement 8
		* - (size + alignement - 1) = 10 + 8 - 1 = 17 ; in binary 1010 + 1000 - 0001 = 10001. same as above
		* - ~(alignement - 1) ; same as above
		* 10001 & ...1000 = 10000. it removed the 1 bit at the end, making it successfully aligned with 1000.
		*/
		return (size + alignment - 1) & ~(alignment - 1);
	}

	/* Uploader */

	struct Uploader
	{
		VkDevice			_VulkanDevice;
		VkCommandBuffer		_CopyBuffer;
		VkQueue				_CopyQueue;
		VkCommandPool		_CopyPool;

		List<VkBuffer>			_ToFreeBuffers;
		List<VkDeviceMemory>	_ToFreeMemory;

		VkPhysicalDeviceMemoryProperties	_MemoryProperties;
	};

	// Creates an Uploader Object to use in all other Vulkan Helper method. will create an open command buffer for copy command and such
	bool StartUploader(class GraphicsAPIManager& GAPI, Uploader& VulkanUploader);

	// Submit the recorded copy commands, wait for it to finish, then releases the copy command memory.
	bool SubmitUploader(Uploader& VulkanUploader);

	/* Shaders */

	bool CreateVulkanShaders(Uploader& VulkanUploader, VkShaderModule& shader, VkShaderStageFlagBits shaderStage, const char* shader_source, const char* shader_name, const char* entry_point = "main");

	/* Memory */

	/*
	* Impose a memory barrier onto the commands in the pipeline for this buffer.
	*
	* Vulkan being a recent API, it is based ongivving developer the most control of the execution of the commands, and with multithreading in mind.
	* It is possible to create and submit GPU command multithreadingly and for the GPU to reschedule the commands to get maximum usage.
	* But it is necessary to synchronise some steps, in a graphics pipeline, this is why mutex exists for CPU threads, Fence for synchronisation of CPU and GPU, and semaphores for GPU commands.
	* Memory Barrier are used to synchronise use of a resource on the GPU, to help the scheduler know when to use or write a resource.
	* CQFD ; this is a way to avoid memory concurrency, and to help schedule work.
	*
	* - dstAccessMask	: how should the resources should be accessed (read/write/etc), and its visibility
	* - syncScopeStart	: define the start in the pipeline of our sync scope. sync scope are the scope in which this resource may be accessed a certain way.
	* - syncScopeEnd	: define the end in the pipeline of our sync scope. sync scope are the scope in which this resource may be accessed a certain way.
	* - srcAccessMask	: what the previous access mask of the resource must be. useful in the instance of overlapping syncScope.
	*/
	void MemorySyncScope(VkCommandBuffer cmdBuffer, VkAccessFlagBits srcAccessMask, VkAccessFlagBits dstAccessMask, VkPipelineStageFlagBits syncScopeStart, VkPipelineStageFlagBits syncScopeEnd);

	uint32_t GetMemoryTypeFromRequirements(const VkMemoryPropertyFlags& wantedMemoryProperties, const VkMemoryRequirements& memoryRequirements, const VkPhysicalDeviceMemoryProperties& memoryProperties);

	/* Allocates GPU memory based on requirements */
	bool AllocateVulkanMemory(Uploader& VulkanUploader, VkMemoryPropertyFlags properties, const VkMemoryRequirements& requirements, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags flags = 0);

	/* Buffers */

	/**
	* A struct in struct of array format used to create the necessary handles and pointers for a glsl shader's "uniform buffer", for each frame in flight.
	*/
	struct UniformBufferHandle
	{
		MultipleScopedMemory<VkBuffer>			_GPUBuffer;
		MultipleScopedMemory<VkDeviceMemory>	_GPUMemoryHandle;
		MultipleScopedMemory<void*>				_CPUMemoryHandle;

		uint32_t _nb_buffer{0};
	};

	/* Creates a one dimensionnal buffer of any usage and the association between CPU and GPU. if AllocateMemory is set to false, bufferMemory MUST BE VALID ! */
	bool CreateVulkanBuffer(Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer);


	/* Allocates memory on GPU dempending on what needsthe buffer given in parameter */
	bool CreateVulkanBufferMemory(Uploader& VulkanUploader, VkMemoryPropertyFlags properties, const VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags flags = 0);


	/* Creates a one dimensionnal buffer of any usage and the association between CPU and GPU. if AllocateMemory is set to false, bufferMemory MUST BE VALID ! */
	bool CreateVulkanBufferAndMemory(Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, uint32_t offset = 0, bool allocate_memory = true, VkMemoryAllocateFlags flags = 0);


	/* Creates all the pointers and handle vulkan needs to create a buffer on the GPU and creates a UniformBufferHandle on the CPU to manage it*/
	bool CreateUniformBufferHandle(Uploader& VulkanUploader, UniformBufferHandle& bufferHandle, uint32_t bufferNb, VkDeviceSize size,
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, bool map_cpu_memory_handle = true);

	/* Asks for de-allocation of all allocated resources for this UniformBufferHandle, on CPU and GPU*/
	void ClearUniformBufferHandle(const VkDevice& VulkanDevice, UniformBufferHandle& bufferHandle);

	/**
	* A struct representing a static and unique buffer on the GPU. You would use this type of buffer for data that will not change between frames such as vertex buffers.
	* include the staging buffer and the static buffer.
	*/
	struct StaticBufferHandle
	{
		VkBuffer		_StaticGPUBuffer{VK_NULL_HANDLE};
		VkDeviceMemory	_StaticGPUMemoryHandle{ VK_NULL_HANDLE };
	};

	/* Creates all the pointers and handle vulkan needs to create a buffer on the GPU and creates a staging buffer to send the data */
	bool CreateStaticBufferHandle(Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, VkDeviceSize size,
		VkBufferUsageFlags staticBufferUsage, VkMemoryPropertyFlags staticBufferProperties = 0, VkMemoryAllocateFlags flags = 0);

	/* sends the data through the staging buffer to the static buffer */
	bool UploadStaticBufferHandle(Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, const void* data, VkDeviceSize size, bool release_staging_buffer = true);

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
				VkBuffer _Positions;
				VkBuffer _Uvs;
				VkBuffer _Normals;
				VkBuffer _Tangents;
			};
			VkBuffer _VertexBuffers[4] = { VK_NULL_HANDLE, VK_NULL_HANDLE , VK_NULL_HANDLE , VK_NULL_HANDLE };
		};

		union
		{
			struct
			{
				VkDeviceSize _pos_offset;
				VkDeviceSize _uv_offset;
				VkDeviceSize _normal_offset;
				VkDeviceSize _tangent_offset;
			};
			VkDeviceSize _vertex_offsets[4] = {0,0,0,0};
		};

		VolatileLoopArray<VkDeviceMemory> _VertexMemoryHandle;

		VkBuffer	_Indices{ VK_NULL_HANDLE };
		uint32_t	_indices_nb;
		uint32_t	_indices_offset;
		VkIndexType _indices_type;

	};

	/* uses tiny obj to load to static buffers into Vulkan */
	void LoadObjFile(Uploader& VulkanUploader, const char* file_name, VolatileLoopArray<Mesh>& meshes);

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
		VolatileLoopArray<Mesh>				_Meshes;
		MultipleVolatileMemory<uint32_t>	_material_index;//same length as mesh

		VolatileLoopArray<VkDeviceMemory>	_BuffersHandle;

		VolatileLoopArray<struct Texture>	_Textures;
		VolatileLoopArray<VkSampler>		_Samplers;

		VolatileLoopArray<struct Material>	_Materials;
	};

	bool LoadGLTFFile(Uploader& VulkanUploader, const char* fileName, Model& meshes);
	void ClearModel(const VkDevice& VulkanDevice, Model& model);

	/* Images */

	/*
	* Impose a memory barrier onto the commands in the pipeline for this image.
	* 
	* Vulkan being a recent API, it is based ongivving developer the most control of the execution of the commands, and with multithreading in mind.
	* It is possible to create and submit GPU command multithreadingly and for the GPU to reschedule the commands to get maximum usage.
	* But it is necessary to synchronise some steps, in a graphics pipeline, this is why mutex exists for CPU threads, Fence for synchronisation of CPU and GPU, and semaphores for GPU commands.
	* Memory Barrier are used to synchronise use of a resource on the GPU, to help the scheduler know when to use or write a resource.
	* CQFD ; this is a way to avoid memory concurrency, and to help schedule work.
	* 
	* - dstImageLayout	: how should the image be used in this scope
	* - dstAccessMask	: how should the image should be accessed (read/write/etc), and its visibility
	* - syncScopeStart	: define the start in the pipeline of our sync scope. sync scope are the scope in which this resource may be accessed a certain way.
	* - syncScopeEnd	: define the end in the pipeline of our sync scope. sync scope are the scope in which this resource may be accessed a certain way.
	* - srcAccessMask	: what the previous access mask of the resource must be. useful in the instance of overlapping syncScope.
	* - srcImageLayout	: what the precious image layout must be. must be informed to preverve the content of the image. otherwise leaving as "undefined" will consider content irrelevant (may be used for framebuffers).
	* - imageRange		: define the range of the content of the image that should be concerned with this barrier. (you may want to put a barrier on a single image of an array)
	*/
	void ImageMemoryBarrier(VkCommandBuffer cmdBuffer, const VkImage& image, VkImageLayout dstImageLayout, VkAccessFlagBits dstAccessMask, VkPipelineStageFlagBits syncScopeStart, VkPipelineStageFlagBits syncScopeEnd, 
		VkAccessFlagBits srcAccessMask = VK_ACCESS_NONE, VkImageLayout srcImageLayout = VK_IMAGE_LAYOUT_UNDEFINED, VkImageSubresourceRange imageRange = { VK_IMAGE_ASPECT_COLOR_BIT , 0, 1, 0, 1});

	/* Allocates memory on GPU dempending on what needs the image given in parameter */
	bool CreateVulkanImageMemory(Uploader& VulkanUploader, VkMemoryPropertyFlags properties, const VkImage& image, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags flags = 0);
	/* creates an image buffer depending on what's given in parameter (no content is being set) */
	bool CreateImage(Uploader& VulkanUploader, VkImage& imageToMake, VkDeviceMemory& imageMemory, uint32_t width, uint32_t height, uint32_t depth, VkImageType imagetype, VkFormat format, VkImageUsageFlags usageFlags, VkMemoryPropertyFlagBits memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, uint32_t offset = 0u, bool allocate_memory = true);
	/* uploads the content given in parameter to the device local image buffer's memory*/
	bool UploadImage(Uploader& VulkanUploader, VkImage& imageToUploadTo, void* image_content, uint32_t width, uint32_t height, uint32_t channels, uint32_t depth = 1);

	/*
	* a struct representing a single texture. it contains everything a shader would need to use said texture meaning :
	* - An Image
	* - An Image View
	* - A sampler (this is user's responsability)
	* Texture is considered a 2D image
	*/
	struct Texture
	{
		VkImage				_Image;
		VkDeviceMemory		_ImageMemory;
		VkImageView			_ImageView;
		VkSampler			_Sampler;
	};

	bool LoadTexture(Uploader& VulkanUploader, const char* file_name, Texture& texture);
	bool LoadTexture(Uploader& VulkanUploader, const void* pixels, uint32_t width, uint32_t height, VkFormat format, Texture& texture);

	void ClearTexture(const VkDevice& VulkanDevice, Texture& texture);


	/*
	* a struct representing an arbitrary material. It contains a texture and the associated descriptor and sampler.
	* Descriptors creation are of the responsability of the pipeline, as you need to have layout to actually use them.
	* You may not actually need to clear as textures and sampler may be the property of other 
	*/
	struct Material
	{
		VolatileLoopArray<Texture>	_Textures;

		VkDescriptorSet				_TextureDescriptors;
	};


	/* Acceleration Structures */

	/*
	* a struct representing a raytraced geometry.
	* it exists to be the representation of a mesh array in the GPU Raytraced Pipeline.
	* In practice, it is a list of Bottom Level Acceleration Structures.
	*/
	struct RaytracedGeometry
	{
		VolatileLoopArray<VkAccelerationStructureKHR>	_AccelerationStructure;
		VolatileLoopArray<VkBuffer>						_AccelerationStructureBuffer;
		VolatileLoopArray<VkDeviceMemory>				_AccelerationStructureMemory;
	};

	/* Creates the Accelerations Structure Buffers and objects, and fills the build info, but does not call build.
	 * /!\ RaytracedGeometry needs to be pre allocated /!\ */
	bool CreateRaytracedGeometry(Uploader& VulkanUploader, const VkAccelerationStructureGeometryKHR& vkGeometry, const VkAccelerationStructureBuildRangeInfoKHR& vkBuildRangeInfo, RaytracedGeometry& raytracedGeometry, VkAccelerationStructureBuildGeometryInfoKHR& vkBuildInfo, uint32_t index = 0);
	/*
	* Creates and Build Acceleration Structure of all mesh in array, with one Acceleration Structure per mesh.
	*/
	bool CreateRaytracedGeometryFromMesh(Uploader& VulkanUploader, RaytracedGeometry& raytracedGeometry, const VolatileLoopArray<Mesh>& mesh);
	/*
	* Creates and builds Accelerations Structure at index of raytracedGeometry from and array of AABBs. 
	* /!\ raytraced geometry must be pre allocated /!\
	*/
	bool CreateRaytracedProceduralFromAABB(Uploader& VulkanUploader, StaticBufferHandle& AABBBuffer, RaytracedGeometry& raytracedGeometry, const MultipleVolatileMemory<VkAabbPositionsKHR>& AABBs, uint32_t nb, uint32_t index = 0);
	void ClearRaytracedGeometry(const VkDevice& VulkanDevice, RaytracedGeometry& raytracedGeometry);

	/*
	* a struct representing a raytraced group.
	* this the representation of a group of raytraced geometry in the GPU Raytraced Pipeline.
	* In practice, it is a Top Level Acceleration Structure, containing reference to multiple Bottom Level AS or Top Level AS.
	*/
	struct RaytracedGroup
	{
		VkAccelerationStructureKHR	_AccelerationStructure;
		VkBuffer					_AccelerationStructureBuffer;
		VkDeviceMemory				_AccelerationStructureMemory;
	};

	/*
	* Creates a Raytraced Group from an array of Raytraced Geometry.
	* Effectively, this creates an instance for each BLAS in each Raytraced Geometry, and groups it in a single TLAS.
	*/
	bool UploadRaytracedGroupFromGeometry(Uploader& VulkanUploader, RaytracedGroup& raytracedObject, const mat4& transform, const MultipleVolatileMemory<RaytracedGeometry*>& geometry, uint32_t nb, bool isUpdate = false);
	bool UploadRaytracedGroupFromGeometry(Uploader& VulkanUploader, RaytracedGroup& raytracedObject, const mat4& transform, const RaytracedGeometry& geometry, bool isUpdate = false);
	void ClearRaytracedGroup(const VkDevice& VulkanDevice, RaytracedGroup& raytracedGeometry);



};

#endif //__VULKAN_HELPER_H__