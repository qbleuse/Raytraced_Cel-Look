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

#define CLEAR_ARRAY(_array, size, call, device) \
	if (_array != nullptr)\
	{\
		for (uint32_t i = 0; i < size; i++)\
			if (_array[i] != nullptr)\
			{\
				call(device, _array[i]); \
				_array[i] = nullptr;\
			}\
		_array.Clear();\
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

#define CLEAR_LIST(_list, size, call, device) \
	if (_list.Nb() > 0)\
	{\
		auto start = _list.GetHead();\
		for (uint32_t i = 0; i < size; i++)\
		{\
			call(device, start->data);\
			start = ++(*start);\
		}\
		_list.Clear();\
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

#define VK_CLEAR_KHR(_array, size, device, call) \
	if (_array != nullptr)\
	{\
		for (uint32_t i = 0; i < size; i++)\
			if (_array[i] != nullptr)\
			{\
				VK_CALL_KHR(device, call, device, _array[i], nullptr); \
				_array[i] = nullptr;\
			}\
		_array.Clear();\
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
struct vec2;
struct vec3;
struct vec4;

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

	/*
	* a struct containging all interfaces needed to create buffer and allocate memory on GPU.
	*/
	struct Uploader
	{
		VkDevice			_VulkanDevice;
		VkCommandBuffer		_CopyBuffer;
		VkQueue				_CopyQueue;
		VkCommandPool		_CopyPool;

		List<VkBuffer>			_ToFreeBuffers;
		List<VkDeviceMemory>	_ToFreeMemory;

		VkPhysicalDeviceMemoryProperties				_MemoryProperties;
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR _RTProperties;
	};

	// Creates an Uploader Object to use in all other Vulkan Helper method. will create an open command buffer for copy command and such
	bool StartUploader(const GraphicsAPIManager& GAPI, Uploader& VulkanUploader);

	/* Creates an Uploader Object to use in all other Vulkan Helper method. 
	* will reuse the runtime command buffer of this frame to create an uplaoder
	* This one is for creation in realtime, using the Runtime Handle.
	* 
	* Memory Properties will not be populated, so memory allocation will not work.
	* if memory allocation is needed, save some parameters during creation time.
	*/
	void StartUploader(const GAPIHandle& GAPIHandle, Uploader& VulkanUploader);

	// Submit the recorded copy commands, wait for it to finish, then releases the copy command memory.
	bool SubmitUploader(Uploader& VulkanUploader);

	/* Shaders */

	/*
	* a struct concatenating all info of a shader.
	*/
	struct ShaderScripts
	{
		MultipleVolatileMemory<char> _ShaderName;
		MultipleVolatileMemory<char> _ShaderSource;

		VkShaderStageFlagBits	_ShaderStage;
		VkShaderModule			_ShaderModule;

	};

	bool CompileVulkanShaders(Uploader& VulkanUploader, VkShaderModule& shader, VkShaderStageFlagBits shaderStage, const char* shader_source, const char* shader_name, const char* entry_point = "main");
	bool CreateVulkanShaders(Uploader& VulkanUploader, ShaderScripts& shader, VkShaderStageFlagBits shaderStage, const char* shader_source, const char* shader_name, const char* entry_point = "main");
	void ClearVulkanShader(const VkDevice& VulkanDevice, ShaderScripts& shader);

	void MakePipelineShaderFromScripts(MultipleScopedMemory<VkPipelineShaderStageCreateInfo>& PipelineShaderStages, const List<ShaderScripts>& ShaderList);


	/*
	* a struct containing the buffer and memory objects of the Shader Binding Table that is created after creating a Raytracing Pipeline Object
	*/
	struct ShaderBindingTable
	{
		VkBuffer		_SBTBuffer{ VK_NULL_HANDLE };
		VkDeviceMemory	_SBTMemory{ VK_NULL_HANDLE };

		union 
		{
			struct
			{
				VkStridedDeviceAddressRegionKHR _RayGenRegion;
				VkStridedDeviceAddressRegionKHR _MissRegion;
				VkStridedDeviceAddressRegionKHR _HitRegion;
				VkStridedDeviceAddressRegionKHR _CallableRegion;
			};

			VkStridedDeviceAddressRegionKHR _SBTRegion[4] = {};
		};
		
	};

	bool GetShaderBindingTable(Uploader& VulkanUploader, const VkPipeline& VulkanPipeline,  ShaderBindingTable& SBT, uint32_t nbMissGroup, uint32_t nbHitGroup, uint32_t nbCallable);
	void ClearShaderBindingTable(const VkDevice& VulkanDevice, ShaderBindingTable& SBT);

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
	bool AllocateVulkanMemory(const Uploader& VulkanUploader, VkMemoryPropertyFlags properties, const VkMemoryRequirements& requirements, uint32_t memoryType, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags flags = 0);

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
	bool CreateVulkanBuffer(const Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buffer);


	/* Allocates memory on GPU dempending on what needsthe buffer given in parameter */
	bool CreateVulkanBufferMemory(const Uploader& VulkanUploader, VkMemoryPropertyFlags properties, const VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkMemoryAllocateFlags flags = 0);

	/* Creates a one dimensionnal buffer of any usage and the association between CPU and GPU. if AllocateMemory is set to false, bufferMemory MUST BE VALID ! */
	bool CreateVulkanBufferAndMemory(const Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, uint32_t offset = 0, bool allocate_memory = true, VkMemoryAllocateFlags flags = 0);

	/* Creates a one dimensional temporary buffer (allocated memory and buffer placed in the ToFree stack of the uploader), and gets the memory address of the buffer */
	bool CreateTmpBufferAndAddress(Uploader& VulkanUploader, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceAddress& tmpBufferAddress, VkMemoryAllocateFlags flags = 0);

	/* Creates all the pointers and handle vulkan needs to create a buffer on the GPU and creates a UniformBufferHandle on the CPU to manage it*/
	bool CreateUniformBufferHandle(Uploader& VulkanUploader, UniformBufferHandle& bufferHandle, uint32_t bufferNb, VkDeviceSize size,
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, bool map_cpu_memory_handle = true, VkMemoryAllocateFlags flags = 0);

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
	bool CreateStaticBufferHandle(const Uploader& VulkanUploader, StaticBufferHandle& bufferHandle, VkDeviceSize size,
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

		union
		{
			struct
			{
				VkDeviceSize _pos_nb;
				VkDeviceSize _uv_nb;
				VkDeviceSize _normal_nb;
				VkDeviceSize _tangent_nb;
			};
			VkDeviceSize _vertex_nb[4] = { 0,0,0,0 };
		};

		VolatileLoopArray<VkDeviceMemory> _VertexMemoryHandle;

		VkBuffer	_Indices{ VK_NULL_HANDLE };
		uint32_t	_indices_nb;
		uint32_t	_indices_offset;

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
	bool CreateModelFromRawVertices(Uploader& VulkanUploader, VolatileLoopArray<vec3>& pos, VolatileLoopArray<vec2>& uv, VolatileLoopArray<vec3>& normal, VolatileLoopArray<vec4>& vertexColor, VolatileLoopArray<uint32_t>& indices, Model& meshes);

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
	bool UploadImage(Uploader& VulkanUploader, VkImage& imageToUploadTo, void* image_content, uint32_t width, uint32_t height, VkFormat format, uint32_t depth = 1);

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
		VkExtent3D			_ImageExtent;
	};

	bool LoadTexture(Uploader& VulkanUploader, const char* file_name, Texture& texture);
	bool LoadTexture(Uploader& VulkanUploader, const void* pixels, uint32_t width, uint32_t height, VkFormat format, Texture& texture);

	bool CreateImage2DArray(Uploader& VulkanUploader, Texture& texture, uint32_t width, uint32_t height, uint32_t layerCount, VkFormat format);

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

	/* Descriptors */

	/* a struct concatenating data of descriptors in pipeline and the descriptor objects themselves */
	struct PipelineDescriptors
	{
		//the binding info for all descriptors
		VolatileLoopArray<VkDescriptorSetLayoutBinding> _DescriptorBindings;

		//a layout object, basically an object that represents all the descriptors needed and their binding
		VkDescriptorSetLayout					_DescriptorLayout;
		//A pool to allocate the descriptor sets needed
		VkDescriptorPool						_DescriptorPool{};
		//the descriptor sets to use (one per frame)
		VolatileLoopArray<VkDescriptorSet>		_DescriptorSets;
	};

	//saves the bindings inside the pipeline descriptor object, and creates the layout object
	bool CreatePipelineDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, const MultipleVolatileMemory<VkDescriptorSetLayoutBinding>& Bindings, uint32_t bindingNb);
	//create the descriptor Pool and sets from the info inside the Pipeline Descriptor
	bool AllocateDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, uint32_t DescriptorSetNb);
	//release the descriptor Pool and sets from the Pipeline Descriptor
	void ReleaseDescriptor(const VkDevice& VulkanDevice, PipelineDescriptors& PipelineDescriptor);
	//uploads an acceleration structure at the descriptor's index given in parameter
	void UploadDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, const VkAccelerationStructureKHR& AS, uint32_t descriptorBindingIndex, uint32_t descriptorSetIndex = 0, uint32_t arrayElement = 0);
	//uploads a buffer at the descriptor's index given in parameter
	void UploadDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, const VkBuffer& Buffer, uint32_t offset, uint32_t range, uint32_t descriptorBindingIndex, uint32_t descriptorSetIndex = 0, uint32_t arrayElement = 0);
	//uploads an image at the descriptor's index given in parameter
	void UploadDescriptor(Uploader& VulkanUploader, PipelineDescriptors& PipelineDescriptor, const VkImageView& ImageView, const VkSampler& Sampler, VkImageLayout ImageLayout, uint32_t descriptorBindingIndex, uint32_t descriptorSetIndex = 0, uint32_t arrayElement = 0);
	//clears all data from the pipeline descriptor object
	void ClearPipelineDescriptor(const VkDevice& VulkanDevice, PipelineDescriptors& PipelineDescriptor);

	/* FrameBuffer */

	struct PipelineOutput
	{
		//the attachement info for the pipeline 
		VolatileLoopArray<VkAttachmentDescription> _OutputColorAttachement;
		VkAttachmentDescription _OutputDepthStencilAttachement;
		//the Clear Value of each attachement if necessary
		VolatileLoopArray<VkClearValue> _OutputClearValue;

		//the render pass that produces the desired output
		VkRenderPass _OutputRenderPass;
		// the viewport that defines the size of the ouput
		VkViewport _OutputViewport;
		// the scissors that defines the size of the ouput
		VkRect2D _OutputScissor;

		VolatileLoopArray<VkFramebuffer> _OuputFrameBuffer;
	};

	//saves the color and depth attachement and creates the associated renderpass
	bool CreatePipelineOutput(Uploader& VulkanUploader, PipelineOutput& PipelineOutput, const MultipleVolatileMemory<VkAttachmentDescription>& colorAttachements, uint32_t colorAttachementNb, const VkAttachmentDescription& depthStencilAttachement);
	// allocates the output framebuffer without creating the objects, and sets value for the viewport and scissor
	void AllocateFrameBuffer(PipelineOutput& PipelineOutput, uint32_t width, uint32_t height, uint32_t framebufferNb);
	// Sets the clear value wanted for an attachement
	void SetClearValue(PipelineOutput& PipelineOutput, const VkClearValue& clearValue, uint32_t clearValueIndex);
	//set images in the frame buffer object
	bool SetFrameBuffer(Uploader& VulkanUploader, PipelineOutput& PipelineOutput, const MultipleVolatileMemory<VkImageView>& framebuffers, uint32_t frameBufferIndex);
	//release the framebuffers objects allocated
	void ReleaseFrameBuffer(const VkDevice& VulkanDevice, PipelineOutput& PipelineOutput);
	//release the memory from all the objects in the Pipeline Output
	void ClearPipelineOutput(const VkDevice& VulkanDevice, PipelineOutput& PipelineOutput);

	/*
	* a struct to represent the content of a single framebuffer duplicated for multiple frames
	*/
	struct FrameBuffer
	{
		//the actual images objects of the framebuffer
		VolatileLoopArray<VkImage> _Images;
		//the render target associated with the images (same number as the images)
		MultipleVolatileMemory<VkImageView> _ImageViews;
		//the GPU memory for the sequentially contained images
		VkDeviceMemory _ImagesMemory;
	};

	//fills up the framebuffer struct depending on the content of the pipeline output (PipelineOutput's framebuffer must be allocated beforehand)
	bool CreateFrameBuffer(Uploader& VulkanUploader, FrameBuffer& Framebuffer, const PipelineOutput& Output, uint32_t associatedAttechementIndex, VkFormat overrideFormat = VK_FORMAT_UNDEFINED);
	//fills up the framebuffer struct using the functions, parameter (meant to be used for compute or raytracing pipline, where there is no need for a pipeline output, but need for framebuffers)
	bool CreateFrameBuffer(Uploader& VulkanUploader, FrameBuffer& Framebuffer, const VkImageCreateInfo& imageInfo, uint32_t frameBufferNb);
	//Clears all the memory allocated by the Framebuffer
	void ClearFrameBuffer(const VkDevice& VulkanDevice, FrameBuffer& Framebuffer);

	/* Acceleration Structures */

	/*
	* a struct representing a raytraced geometry.
	* it exists to be the representation of a mesh array in the GPU Raytraced Pipeline.
	* In practice, it is a list of Bottom Level Acceleration Structures.
	*/
	struct RaytracedGeometry
	{
		//model info

		VolatileLoopArray<VkAccelerationStructureKHR>	_AccelerationStructure;
		VolatileLoopArray<VkBuffer>						_AccelerationStructureBuffer;
		VolatileLoopArray<VkDeviceMemory>				_AccelerationStructureMemory;

		//material info

		VolatileLoopArray<uint32_t>						_CustomInstanceIndex;
		VolatileLoopArray<uint32_t>						_ShaderOffset;
	};

	/* Creates the Accelerations Structure Buffers and objects, and fills the build info, but does not call build.
	 * /!\ RaytracedGeometry needs to be pre allocated /!\ */
	bool CreateRaytracedGeometry(Uploader& VulkanUploader, const VkAccelerationStructureGeometryKHR& vkGeometry, const VkAccelerationStructureBuildRangeInfoKHR& vkBuildRangeInfo, RaytracedGeometry& raytracedGeometry, VkAccelerationStructureBuildGeometryInfoKHR& vkBuildInfo, uint32_t index = 0, uint32_t customInstanceIndex = 0, uint32_t shaderOffset = 0);
	/*
	* Creates and Build Acceleration Structure of all mesh in array, with one Acceleration Structure per mesh.
	* 
	* Each acceleration structure may have a transform associated:
	* - it must be one buffer for each transform of each mesh. if it is null, we will consider that there is no transform
	* - if offset is nullptr, we consider that every mesh uses the same transform. if it is not, offset must be the same size as mesh.
	*/
	bool CreateRaytracedGeometryFromMesh(Uploader& VulkanUploader, RaytracedGeometry& raytracedGeometry, const VolatileLoopArray<Mesh>& mesh, const VkBuffer& transformBuffer = VK_NULL_HANDLE, const MultipleVolatileMemory<uint32_t>& transformOffset = nullptr, const MultipleVolatileMemory<uint32_t>& customInstanceIndex = nullptr, const MultipleVolatileMemory<uint32_t>& shaderOffset = nullptr);
	/*
	* Creates and builds Accelerations Structure at index of raytracedGeometry from and array of AABBs. 
	* /!\ raytraced geometry must be pre allocated /!\
	*/
	bool CreateRaytracedProceduralFromAABB(Uploader& VulkanUploader, StaticBufferHandle& AABBBuffer, RaytracedGeometry& raytracedGeometry, const MultipleVolatileMemory<VkAabbPositionsKHR>& AABBs, uint32_t nb, uint32_t index = 0, uint32_t customInstanceIndex = 0, uint32_t shaderOffset = 0);
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

		//this needs to be kept for updates
		VkAccelerationStructureBuildGeometryInfoKHR						_InstancesInfo{};
		SingleScopedMemory<VkAccelerationStructureBuildRangeInfoKHR>	_InstancesRange;
		MultipleScopedMemory<VkAccelerationStructureInstanceKHR>		_Instances;
		StaticBufferHandle												_InstancesBufferHandle;


		//the memory type of the scratch buffer
		uint32_t _ScratchMemoryType;
	};

	/*
	* Creates a Raytraced Group from an array of Raytraced Geometry.
	* Effectively, this creates an instance for each BLAS in each Raytraced Geometry, and groups it in a single TLAS.
	* 
	* Each BLAS in a raytraced geometry will be created with the same instance parameters.
	* Each RaytracedGeometry may have different shaderGroupIndex and customInstanceIndex.
	* However, they will all share the same transform.
	* 
	* - transform				: the mat4 transformation matrix of all the instances in this group
	* - geometry				: the array of raytraced geometry to make into instances and group into a single TLAS. should be the size of nb.
	* - nb						: the nb of raytraced geometry in the geometry array. NOT THE TOTAL NB OF BLAS, JUST THE NUMBER OF RAYTRACED GEOMETRY.
	*/
	bool CreateRaytracedGroupFromGeometry(Uploader& VulkanUploader, RaytracedGroup& raytracedObject, const mat4& transform, const MultipleVolatileMemory<RaytracedGeometry*>& geometry, uint32_t nb);
	/* populates the Instances parameters of the raytraced group */
	bool CreateInstanceFromGeometry(Uploader& VulkanUploader, RaytracedGroup& raytracedObject, const mat4& transform, const MultipleVolatileMemory<RaytracedGeometry*>& geometry, uint32_t nb);
	/* Updates the transform for the specfied nb of instances, starting from index. */
	bool UpdateTransform(const VkDevice& VulkanDevice, RaytracedGroup& raytracedObject, const mat4& transform, uint32_t index, uint32_t nb);
	bool UpdateRaytracedGroup(Uploader& VulkanUploader, RaytracedGroup& raytracedGroup);
	void ClearRaytracedGroup(const VkDevice& VulkanDevice, RaytracedGroup& raytracedGroup);

	/* Scene Representation */


	struct SceneMaterials
	{
		//color texture array
		Texture _Textures;
	};


	struct SceneBuffer
	{
		//copy of the index buffer of all primitives in the scene
		StaticBufferHandle _IndexBuffer;
		//copy of the uv buffer of all primitives in the scene
		StaticBufferHandle _UVsBuffer;
		//copy of the normal buffer of all primitives in the scene
		StaticBufferHandle _NormalBuffer;

		//an offset buffer containing information on which part of the scene buffer to pick when rendering in shader
		StaticBufferHandle _OffsetBuffer;

		//the object holding all texture info for all models
		Texture	_TextureArray;

		//the array containing all the samplers for each textures
		VolatileLoopArray<VkSampler> _TextureSamplers;

		uint32_t _IndexBufferSize	= 0;
		uint32_t _UVsBufferSize		= 0;
		uint32_t _NormalBufferSize	= 0;

		uint32_t _OffsetBufferSize = 0;
	};

	bool CreateSceneBufferFromMeshes(Uploader& VulkanUploader, SceneBuffer& sceneBuffer, const VolatileLoopArray<Mesh>& mesh);
	bool CreateSceneBufferFromModels(Uploader& VulkanUploader, SceneBuffer& sceneBuffer, const MultipleVolatileMemory<Model>& model, uint32_t modelNb);



	void ClearSceneBuffer(const VkDevice& VulkanDevice, SceneBuffer& sceneBuffer);



};

#endif //__VULKAN_HELPER_H__