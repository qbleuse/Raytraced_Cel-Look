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
#include "Define.h"


/**
* This class is a scene to raytrace using GPU raytracing a model.
*/
class RaytraceGPU : public Scene
{

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

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
	VulkanHelper::FrameBuffer		_RayFramebuffer;

	//a uniform buffer for matrices data
	VulkanHelper::UniformBufferHandle	_RayUniformBuffer;
	//a struct containing the preallocated Vulkan memory and buffer of a loaded model
	VulkanHelper::Model					_RayModel;
	//static buffers concatenating all primitive data in the scene for use in the shader
	VulkanHelper::SceneBuffer			_RaySceneBuffer;
	//the 3D geometry of the model in the GPU raytracing pipeline format (BLAS)
	VulkanHelper::RaytracedGeometry		_RayBottomAS;

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

	/*
	* Compiles the shaders to use in the Raytracing Pipeline Object creation
	*/
	void PrepareVulkanRaytracingScripts(class GraphicsAPIManager& GAPI);

	//This scene's pipeline layout, defining all the descriptors our pipeline will need
	VkPipelineLayout					_CopyLayout{ VK_NULL_HANDLE };
	//This scene's pipeline, defining the behaviour of all fixed function alongside the render pass and shaders.
	VkPipeline							_CopyPipeline{ VK_NULL_HANDLE };

	//sampler used to sample out the Raytraced Write Image to our framebuffer
	VkSampler							_CopySampler{ VK_NULL_HANDLE };
	//the shaders used for the fullscreen copy
	List<VulkanHelper::ShaderScripts>	_CopyShaders;


	//an object describing the ouput attached to the copy pipeline (such as frame buffer)
	VulkanHelper::PipelineOutput	_CopyPipelineOutput;
	//the descriptor needed for the copy pipeline (such as the sampler)
	VulkanHelper::PipelineDescriptors _CopyDescriptors;

	/*
	* Creates the necessary resources for copying teh raytraced image with vulkan.
	* This includes:
	* - RenderPass Object
	* - DescriptorSet Layouts
	* - Pipeline Layout
	* - Pipeline Object
	* - DescriptorSets Objects for the model's samplers (using Descriptor Pool)
	*/
	void PrepareVulkanProps(class GraphicsAPIManager& GAPI);

	/*
	* Creates a fullscreen Pipeline object using the Shaders and pipeline layout given in parameter.
	*/
	void CreateFullscreenCopyPipeline(class GraphicsAPIManager& GAPI, VkPipeline& Pipeline, const VkPipelineLayout& PipelineLayout, const VkRenderPass& RenderPass, const List<VulkanHelper::ShaderScripts>& Shaders);

	/*
	* Compiles the shaders to use in the Copy Pipeline Object creation
	*/
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI);


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
	
	struct ObjectData
	{
		Transform			_Trs;
		bool				_ChangedFlag = false;
	};
	//a struct to group the data that can be changed by user
	ObjectData _RayObjData;

	struct UniformBuffer
	{
		mat4				_view;
		mat4				_proj;
		RaytracingParams	_rt_params;
		uint32_t			_nb_frame;
		float				_random;
	};
	//a buffer to allow sending the data changed by user at once
	UniformBuffer _RayBuffer;


	/*===== END Scene Interface =====*/
#pragma endregion 


};

#endif //__RAYTRACE_GPU_H__