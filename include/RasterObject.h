#ifndef __RASTER_OBJECT_H__
#define __RASTER_OBJECT_H__


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
* This class is a scene to try to load a model and do all the thing a normal forward rasterized renderer would do.
*/
class RasterObject : public Scene
{

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//This scene's renderpass, defining what is attached to the pipeline, and what is outputed
	VkRenderPass				_ObjRenderPass{};
	//This scene's descriptor uniform buffer layout, defining how to use the uniform buffer
	VkDescriptorSetLayout		_ObjBufferDescriptorLayout{};
	//This scene's descriptor sampler layout, defining how to use the sampler
	VkDescriptorSetLayout		_ObjSamplerDescriptorLayout{};
	//This scene's pipeline layout, defining all the descriptors our pipeline will need (uniform and sampler, basically)
	VkPipelineLayout			_ObjLayout{};
	//This scene's pipeline, defining the behaviour of all fixed function alongside the render pass and shaders.
	VkPipeline					_ObjPipeline{};

	//our scene's viewport (usually takes the whole screen. can change at runtime, as window can be resized)
	VkViewport					_ObjViewport{};
	//our scene's scissors (usually takes the whole screen, therefore not doing any cuts)
	VkRect2D					_ObjScissors{};

	//The outputs attached to the scene's pipeline as described in the renderpass. Here, back buffer and depth.
	MultipleScopedMemory<VkFramebuffer>		_ObjOutput;

	//A pool to allocate the descriptor needed to use our uniform buffers
	VkDescriptorPool						_ObjBufferDescriptorPool{};
	//the descriptor to use our Uniform Buffers (one per frame)
	MultipleScopedMemory<VkDescriptorSet>	_ObjBufferDescriptorSet;

	//A pool to allocate the descriptor needed to use our samplers
	VkDescriptorPool						_ObjSamplerDescriptorPool{};
	//the descriptor to use our samplers (one per texture of the loaded model)
	MultipleScopedMemory<VkDescriptorSet>	_ObjSamplerDescriptorSet;


	//A handle containing the 3D matrices data
	VulkanHelper::UniformBufferHandle	_ObjMatBufferHandle;
	//a struct containing the preallocated Vulkan memory and buffer of a loaded model
	VulkanHelper::Model					_ObjModel;

	/*
	* Creates the necessary resources for displaying with vulkan.
	* This includes:
	* - RenderPass Object
	* - DescriptorSet Layouts
	* - Pipeline Layout
	* - Pipeline Object
	* - the Model
	* - DescriptorSets Objects for the model's samplers (using Descriptor Pool)
	*/
	void PrepareVulkanProps(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	
	/*
	* Compiles the shaders to use in the Pipeline Object creation
	*/
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	
	/*
	* Deallocate previously allocated resources, then recreate resources using the window's new properties.
	* This includes:
	* - Viewport (though not allocated)
	* - Scissors (though not allocated)
	* - Framebuffers
	* - DescriptorSets Objects for the matrices (using Descriptor Pool)
	* - 3D matrices Uniform Buffer
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
	* Allocates the resources associated with window (which can change size during runtime), and deallocate previously allocated resources if needed.
	* This is expected to be called multiple time, do not put resources that are supposed to be created once in this method.
	*/
	virtual void Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)final;

	/*
	* The name of this scene. This can be hardcoded or a member, but it should never fail.
	*/
	__forceinline virtual const char* Name()const noexcept override { return "Raster Object"; }

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

	
	struct ObjectData
	{
		Transform _Trs;
	};
	//a struct to group the data that can be changed by user
	ObjectData _ObjData;

	struct UniformBuffer
	{
		mat4 model;
		mat4 view;
		mat4 proj;
	};
	//a buffer to allow sending the data changed by user at once
	UniformBuffer _ObjBuffer;


	/*===== END Scene Interface =====*/
#pragma endregion 


};

#endif //__RASTER_OBJECT_H__