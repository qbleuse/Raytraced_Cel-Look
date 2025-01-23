#ifndef __DEFFERED_RENDERING_H__
#define __DEFFERED_RENDERING_H__

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
* This class is a scene to make a deffered renderer.
*/
class DefferedRendering : public Scene
{

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//the model/scene to render for this scene
	VulkanHelper::Model	_Model;
	//the descriptor needed for the scene's model
	VulkanHelper::PipelineDescriptors	_ModelDescriptors;

	/* G BUFFER */

	//The GBuffer pipeline layout, defining all the descriptors our pipeline will need
	VkPipelineLayout					_GBUfferLayout{ VK_NULL_HANDLE };
	//The GBUffer pipeline, defining the behaviour of all fixed function alongside the render pass and shaders.
	VkPipeline							_GBufferPipeline{ VK_NULL_HANDLE };

	//the GBuffer pass's uniform buffer for the model
	VulkanHelper::UniformBufferHandle	_GBUfferUniformBuffer;

	//the shaders used for the G Buffer Pass
	List<VulkanHelper::ShaderScripts>	_GBufferShaders;

	//an object describing the ouput attached to the deffered pipeline (such as frame buffer)
	VulkanHelper::PipelineOutput		_GBUfferPipelineOutput;
	//the descriptor needed for the deffered pipeline (such as the Uniform Buffer)
	VulkanHelper::PipelineDescriptors	_GBufferDescriptors;

	//the actual G Buffers used for deffered rendering
	MultipleScopedMemory<VulkanHelper::FrameBuffer>	_GBuffers;

	/* DEFFERED COMPOSITING */

	//The deffered pipeline layout, defining all the descriptors our pipeline will need
	VkPipelineLayout					_DefferedLayout{ VK_NULL_HANDLE };
	//The deffered pipeline, defining the behaviour of all fixed function alongside the render pass and shaders.
	VkPipeline							_DefferedPipeline{ VK_NULL_HANDLE };

	//sampler used to sample out the GBUffers
	VkSampler							_DefferedSampler{ VK_NULL_HANDLE };
	//the shaders used for the deffered pass
	List<VulkanHelper::ShaderScripts>	_DefferedShaders;

	//an object describing the ouput attached to the copy pipeline (such as frame buffer)
	VulkanHelper::PipelineOutput		_DefferedPipelineOutput;
	//the descriptor needed for the copy pipeline (such as the sampler)
	VulkanHelper::PipelineDescriptors	_DefferedDescriptors;

	/*
	* Creates the necessary resources for deffered rendering of a scene.
	* This includes:
	* - RenderPass Object
	* - DescriptorSet Layouts
	* - Pipeline Layout
	* - Pipeline Object
	* - DescriptorSets Objects for the model's samplers (using Descriptor Pool)
	*/
	void PrepareVulkanProps(class GraphicsAPIManager& GAPI);

	/*
	* Creates a model render Pipeline object using the Shaders and pipeline layout given in parameter.
	*/
	void CreateModelRenderPipeline(class GraphicsAPIManager& GAPI, VkPipeline& Pipeline, const VkPipelineLayout& PipelineLayout, const VulkanHelper::PipelineOutput& PipelineOutput, const List<VulkanHelper::ShaderScripts>& Shaders);

	/*
	* Creates a fullscreen Pipeline object using the Shaders and pipeline layout given in parameter.
	*/
	void CreateFullscreenCopyPipeline(class GraphicsAPIManager& GAPI, VkPipeline& Pipeline, const VkPipelineLayout& PipelineLayout, const VulkanHelper::PipelineOutput& PipelineOutput, const List<VulkanHelper::ShaderScripts>& Shaders);

	/*
	* Compiles the shaders to use in the Copy Pipeline Object creation
	*/
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI);

	/*
	* Deallocate previously allocated resources, then recreate resources using the window's new properties.
	* This includes:
	* - Viewport (though not allocated)
	* - Scissors (though not allocated)
	* - Framebuffers
	* - Descriptor sets for out put raytracing output images as asampled image
	*/
	void ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames);


	/*
	* starts render pass, and binds the necessary resources to the pipeline
	*/
	static void BindPass(GAPIHandle& GAPIHandle, const VkPipeline& Pipeline,
		const VulkanHelper::PipelineOutput& PipelineOutput);

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
	virtual void Prepare(class GraphicsAPIManager& GAPI)override;

	/*
	* Allocates the resources associated with window (which can change size during runtime), and deallocate previously allocated resources if needed.
	* This is expected to be called multiple time, do not put resources that are supposed to be created once in this method.
	*/
	virtual void Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)override;

	/*
	* The name of this scene. This can be hardcoded or a member, but it should never fail.
	*/
	__forceinline virtual const char* Name()const noexcept override { return "Deffered Rendering"; }

	/*
	* called once per frame.
	* Changes data inside the scene depending on time or user input that can be found in the AppContext (or with ImGUI)
	* UI should also be updated here.
	*/
	virtual void Act(struct AppWideContext& AppContext)override;

	/*
	* called once per frame.
	* Uses the data changed in Act and the resources to display on screen the scene's current context.
	* Should create and send the render command to the GPU.
	*/
	virtual void Show(GAPIHandle& GAPIHandle)override;

	/*
	* Releases all the resources allocated in Prepare
	*/
	virtual void Close(class GraphicsAPIManager& GAPI)override;

	struct ObjectData
	{
		vec3 pos;
		vec3 euler_angles;
		vec3 scale;
	};
	//a struct to group the data that can be changed by user
	ObjectData _ObjData;
	bool changedFlag = false;

	struct UniformBuffer
	{
		mat4 _model;
		mat4 _view;
		mat4 _proj;
	};
	//a buffer to allow sending the data changed by user at once
	UniformBuffer _UniformBuffer;


	/*===== END Scene Interface =====*/
#pragma endregion 


};


#endif //__DEFFERED_RENDERING_H__