#ifndef __RAYTRACE_CEL_H__
#define __RAYTRACE_CEL_H__


//include parent
#include "DefferedRendering.h"
#include "Define.h"

/**
* This class is a scene to attempt at stylized, cel-look, realtime raytracing.
*/
class RaytracedCel : public DefferedRendering
{

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//a model containing a CornellBox
	VulkanHelper::Model _CornellBox;
	//the 3D geometry of the CornellBox in the GPU raytracing pipeline format (BLAS)
	VulkanHelper::RaytracedGeometry		_CornellBoxBottomAS;
	//the descriptors for the ccornell box
	VulkanHelper::PipelineDescriptors	_CornellBoxDescriptor;


	//the semaphore to synchronize between G Buffer work and raytracing/compositing work
	MultipleScopedMemory<VkSemaphore>	_GBufferDrawnSemaphore;

	//the layouts and descriptors of this scene that can change in real time
	VulkanHelper::PipelineDescriptors _RayPipelineDynamicDescriptor;
	//the layouts and descriptors of this scene that does not change
	VulkanHelper::PipelineDescriptors _RayPipelineStaticDescriptor;
	//This scene's raytracing pipeline layout, defining all the descriptors our raytracing pipeline will need (basically the above descriptor's copy)
	VkPipelineLayout			_RayLayout{};
	//This scene's raytracing pipeline, defining shader groups.
	VkPipeline					_RayPipeline{};

	//The binding table creatd from the raytracing shader
	VulkanHelper::ShaderBindingTable	_RayShaderBindingTable;
	List<VulkanHelper::ShaderScripts>	_RayShaders;

	//the image to write to in the raytracing pipeline
	MultipleScopedMemory<VulkanHelper::FrameBuffer>	_RayFramebuffers;

	//a uniform buffer for matrices data
	VulkanHelper::UniformBufferHandle	_RayUniformBuffer;
	//static buffers concatenating all primitive data in the scene for use in the shader
	VulkanHelper::SceneBuffer			_RaySceneBuffer;
	//the 3D geometry of the model in the GPU raytracing pipeline format (BLAS)
	VulkanHelper::RaytracedGeometry		_RayBottomAS;

	//the Instance encapsulating the raytraced gemometry (TLAS)
	VulkanHelper::RaytracedGroup		_RayTopAS;

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
	void PrepareVulkanRaytracingProps(class GraphicsAPIManager& GAPI);


	//Creates the model and the other necessary resources 
	virtual void PrepareModelProps(class GraphicsAPIManager& GAPI)override;

	//Creates the deffered compositing pass necessary resources 
	virtual void PrepareDefferedPassProps(class GraphicsAPIManager& GAPI)override;

	/*
	* Compiles the shaders to use in the Raytracing Pipeline Object creation
	*/
	void PrepareVulkanRaytracingScripts(class GraphicsAPIManager& GAPI);

	//compiles the shader for a deffered compositing pass using the raytraced light framebuffer
	virtual void PrepareCompositingScripts(class GraphicsAPIManager& GAPI)override;

	/*
	* Deallocate previously allocated raytracing resources, then recreate resources using the window's new properties.
	* This includes:
	* - DescriptorSets Objects for the Acceleration Structures
	* - Output Images
	*/
	void ResizeVulkanRaytracingResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames);


	/* DirectX */

	/* Metal */


	/*===== END Graphics API =====*/
#pragma endregion

#pragma region SCENE INTERFACE 
public:
	/*===== Scene Interface =====*/

	/*
	* Imports all the data user can change, saved from each use.
	*/
	virtual void Import(rapidjspon::Document& AppSettings)final;

	/*
	* Exports all the data user can change in the document, to save it for next use.
	*/
	virtual void Export(rapidjspon::Document& AppSettings)final;

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
	__forceinline virtual const char* Name()const noexcept override { return "Raytraced Cel-Shading"; }

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

	struct RayBuffer
	{
		mat4 _view;
		mat4 _proj;
		RaytracingParams _rtParams;
		uint32_t _nb_frame;
		float	_random;
	};
	//a buffer to allow sending the data changed by user at once
	RayBuffer _RayBuffer;


	/*===== END Scene Interface =====*/
#pragma endregion 


};

#endif //__RAYTRACE_CEL_H__