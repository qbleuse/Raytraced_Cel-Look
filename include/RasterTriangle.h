#ifndef __RASTER_TRIANGLE_H__
#define __RASTER_TRIANGLE_H__


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
* This class is the beginning and first scene of all good Graphics Project : a Rasterized Triangle !
*/
class RasterTriangle : public Scene
{

	struct UniformBuffer
	{
		vec4 first;
		vec4 second;
		vec4 third;
		vec4 padding;
	};

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//the viewport of this scene (can change at runtime, as window can be resized)

	//This scene's renderpass, defining what is attached to the pipeline, and what is outputed
	VkRenderPass				_TriRenderPass{VK_NULL_HANDLE};
	//This scene's descriptor layout, defining how to use our resources
	VkDescriptorSetLayout		_TriVertexDescriptorLayout{ VK_NULL_HANDLE };
	//This scene's pipeline layout, defining all the descriptors our pipeline will need
	VkPipelineLayout			_TriLayout{ VK_NULL_HANDLE };
	//This scene's pipeline, defining the behaviour of all fixed function alongside the render pass and shaders.
	VkPipeline					_TriPipeline{ VK_NULL_HANDLE };
	
	//our scene's viewport (usually takes the whole screen)
	VkViewport					_TriViewport{};
	//our scene's scissors (usually takes the whole screen, therefore not doing any cuts)
	VkRect2D					_TriScissors{};

	//The outputs attached to the scene's pipeline as described in the renderpass
	MultipleScopedMemory<VkFramebuffer>		_TriOutput;
	//A pool to allocate the descriptor needed to use our uniform buffers
	VkDescriptorPool						_TriDescriptorPool{ VK_NULL_HANDLE };
	//the descriptor to use our Uniform Buffers
	MultipleScopedMemory<VkDescriptorSet>	_TriVertexDescriptorSet;

	//A handle containing the vertices data
	VulkanHelper::UniformBufferHandle	_TriPointsHandle;
	//A handle containing the colour data
	VulkanHelper::UniformBufferHandle	_TriColourHandle;


	/*
	* Creates the necessary resources for displaying with vulkan.
	* This includes:
	* - RenderPass Object
	* - DescriptorSet Layout
	* - Pipeline Layout
	* - Pipeline Object
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
	* - DescriptorSets Objects (using Descriptor Pool)
	* - Uniform Buffers
	*/
	void ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames);


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
	__forceinline virtual const char* Name()const noexcept override { return "Raster Triangle"; }

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

	//flag to determine if user has changed something in the buffers
	bool _changed = true;

	//buffer that contains the three vertices coordinates of the triangle
	UniformBuffer _PointBuffer;
	//buffer that contains the colour of the three vertices of the triangle
	UniformBuffer _ColourBuffer;



	/*===== END Scene Interface =====*/
#pragma endregion 


};

#endif //__RASTER_TRIANGLE_H__