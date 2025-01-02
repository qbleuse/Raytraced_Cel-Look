#ifndef __RAYTRACE_GPU_H__
#define __RAYTRACE_GPU_H__


//include parent
#include "Scene.h"

//Graphics API
#include "VulkanHelper.h"

//for imgui and context
#include "AppWideContext.h"

//utilities include
#include "Utilities.h"
#include "Maths.h"

/**
* This class is a scene to raytrace using GPU raytracing a model.
*/
class RaytraceGPU : public Scene
{

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//This scene's raytracing descriptor layout, defining how to use the AS and what image to store the result in
	VkDescriptorSetLayout		_RayDescriptorLayout{};
	//This scene's raytracing pipeline layout, defining all the descriptors our raytracing pipeline will need (basically the above descriptor's copy)
	VkPipelineLayout			_RayLayout{};
	//This scene's raytracing pipeline, defining shader groups.
	VkPipeline					_RayPipeline{};

	//The binding table creatd from the raytracing shader
	MultipleScopedMemory<VkStridedDeviceAddressRegionKHR>	_RayShaderBindingAddress;
	//the buffer for the shader binding table
	VkBuffer												_RayShaderBindingBuffer{ VK_NULL_HANDLE };
	//the memory for the shader binding table
	VkDeviceMemory											_RayShaderBindingMemory{VK_NULL_HANDLE};

	//A pool to allocate the descriptor sets needed
	VkDescriptorPool						_RayDescriptorPool{};
	//the descriptor sets to use (one per frame)
	MultipleScopedMemory<VkDescriptorSet>	_RayDescriptorSet;

	//the image we will write on with the raytracing pipeline
	ScopedLoopArray<VkImage>		_RayWriteImage;
	//the image view associated with our image to write.
	ScopedLoopArray<VkImageView>	_RayWriteImageView;
	//the allocated memory on GPU in which the local image is
	VkDeviceMemory					_RayImageMemory{ VK_NULL_HANDLE };

	//a uniform buffer for matrices data
	VulkanHelper::UniformBufferHandle	_RayUniformBuffer;
	//a struct containing the preallocated Vulkan memory and buffer of a loaded model
	VulkanHelper::Model					_RayModel;
	//the 3D geometry of the model in the GPU raytracing pipeline format (BLAS)
	VulkanHelper::RaytracedGeometry		_RayBottomAS;
	//the Instance encapsulating the raytraced gemometry (TLAS)
	VulkanHelper::RaytracedGroup		_RayTopAS;

	//the Buffer describing the AABB needed for sphere's procedural geometry
	MultipleScopedMemory<VulkanHelper::StaticBufferHandle>	_RaySphereAABBBuffer;
	//the buffer containing the sphere's instances
	VulkanHelper::StaticBufferHandle	_RaySphereBuffer;
	//the buffer containing the sphere's material
	VulkanHelper::StaticBufferHandle	_RaySphereColour;
	//the buffer containing the sphere's offset to read the buffer's above
	VulkanHelper::StaticBufferHandle	_RaySphereOffsets;
	//the procedural spheres in the GPU raytracing pipeline format (BLAS)
	VulkanHelper::RaytracedGeometry		_RaySphereBottomAS;
	//the Instance encapsulating the raytraced gemometry (TLAS)
	VulkanHelper::RaytracedGroup		_RaySphereTopAS;

	/*
	* Creates the necessary resources for raytracing with vulkan.
	* This includes:
	* - RenderPass Object
	* - DescriptorSet Layouts
	* - Pipeline Layout
	* - Pipeline Object
	* - the Model
	* - DescriptorSets Objects for the model's samplers (using Descriptor Pool)
	*/
	void PrepareVulkanRaytracingProps(class GraphicsAPIManager& GAPI, VkShaderModule& RayGenShader, VkShaderModule& MissShader, VkShaderModule& TriangleHitShader, VkShaderModule& HitShader, VkShaderModule& IntersectShader, VkShaderModule& DiffuseShader, VkShaderModule& MetalShader, VkShaderModule& Dieletrics);

	/*
	* Compiles the shaders to use in the Raytracing Pipeline Object creation
	*/
	void PrepareVulkanRaytracingScripts(class GraphicsAPIManager& GAPI, VkShaderModule& RayGenShader, VkShaderModule& MissShader, VkShaderModule& TriangleHitShader, VkShaderModule& HitShader, VkShaderModule& IntersectShader, VkShaderModule& DiffuseShader, VkShaderModule& MetalShader, VkShaderModule& Dieletrics);

	//This scene's renderpass, defining what is attached to the pipeline, and what is outputed
	VkRenderPass				_CopyRenderPass{ VK_NULL_HANDLE };
	//This scene's descriptor layout, defining how to use our resources
	VkDescriptorSetLayout		_CopyDescriptorLayout{ VK_NULL_HANDLE };
	//This scene's pipeline layout, defining all the descriptors our pipeline will need
	VkPipelineLayout			_CopyLayout{ VK_NULL_HANDLE };
	//This scene's pipeline, defining the behaviour of all fixed function alongside the render pass and shaders.
	VkPipeline					_CopyPipeline{ VK_NULL_HANDLE };

	//our scene's viewport (usually takes the whole screen)
	VkViewport					_CopyViewport{};
	//our scene's scissors (usually takes the whole screen, therefore not doing any cuts)
	VkRect2D					_CopyScissors{};

	//The outputs attached to the scene's pipeline as described in the renderpass
	MultipleScopedMemory<VkFramebuffer>		_CopyOutput;
	//A pool to allocate the descriptor needed to use our uniform buffers
	VkDescriptorPool						_CopyDescriptorPool{ VK_NULL_HANDLE };
	//the descriptor to use our Uniform Buffers
	MultipleScopedMemory<VkDescriptorSet>	_CopyDescriptorSet;
	//sampler used to sample out the Raytraced Write Image to our framebuffer
	VkSampler								_CopySampler{ VK_NULL_HANDLE };


	/*
	* Creates the necessary resources for copying teh raytraced image with vulkan.
	* This includes:
	* - RenderPass Object
	* - DescriptorSet Layouts
	* - Pipeline Layout
	* - Pipeline Object
	* - DescriptorSets Objects for the model's samplers (using Descriptor Pool)
	*/
	void PrepareVulkanProps(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);

	/*
	* Compiles the shaders to use in the Copy Pipeline Object creation
	*/
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);


	/*
	* Deallocate previously allocated raytracing resources, then recreate resources using the window's new properties.
	* This includes:
	* - DescriptorSets Objects for the Acceleration Structures
	* - Output Images
	*/
	void ResizeVulkanRaytracingResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames);

	/*
	* Deallocate previously allocated resources, then recreate resources using the window's new properties.
	* This includes:
	* - Viewport (though not allocated)
	* - Scissors (though not allocated)
	* - Framebuffers
	* - Descriptor sets for out put raytracing output images as asampled image
	*/
	void ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames);


	/* DirectX */

	/* Metal */


	/*===== END Graphics API =====*/
#pragma endregion

#pragma region SCENE INTERFACE 
public:
	/*===== Scene Interface =====*/

	/*
	* Prepares once all the unmovable resources needed (such as pipelines in recent Graphics APIs)
	*/
	virtual void Prepare(class GraphicsAPIManager& GAPI)final;

	/*
	* Generates spheres in the GPU raytraced sphere as procedural objects.
	*/
	void GenerateSpheres(class GraphicsAPIManager& GAPI);

	/*
	* Allocates the resources associated with window (which can change size during runtime), and deallocate previously allocated resources if needed.
	* This is expected to be called multiple time, do not put resources that are supposed to be created once in this method.
	*/
	virtual void Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)final;

	/*
	* The name of this scene. This can be hardcoded or a member, but it should never fail.
	*/
	__forceinline virtual const char* Name()const noexcept override { return "Raytrace GPU"; }

	/*
	* called once per frame.
	* Changes data inside the scene depending on time or user input that can be found in the AppContext (or with ImGUI)
	* UI should also be updated here.
	*/
	virtual void Act(struct AppWideContext& AppContext)final;

	/*
	* called once per frame.
	* Uses the data changed in Act and the resources to display on screen the scene's current context.
	* Should create and send the render command to the GPU.
	*/
	virtual void Show(GAPIHandle& GAPIHandle)final;

	/*
	* Releases all the resources allocated in Prepare
	*/
	virtual void Close(class GraphicsAPIManager& GAPI)final;

	struct UniformBuffer
	{
		mat4 model;
		mat4 view;
		mat4 proj;
	};
	//a buffer to allow sending the data changed by user at once
	UniformBuffer _RayBuffer;


	/*===== END Scene Interface =====*/
#pragma endregion 


};

#endif //__RAYTRACE_GPU_H__