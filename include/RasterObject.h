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
* This class is the beginning and first scene of all good Graphics Project : a Rasterized Triangle !
*/
class RasterObject : public Scene
{

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//the viewport of this scene (can change at runtime, as window can be resized)
	VkViewport					objectViewport{};
	VkRect2D					objectScissors{};
	VkRenderPass				objectRenderPass{};
	VkDescriptorSetLayout		objectVertexDescriptorLayout{};
	VkDescriptorPool			objectDescriptorPool{};
	VkPipelineLayout			objectLayout{};
	VkPipeline					objectPipeline{};

	SimpleArray<VkFramebuffer>		objectOutput;
	SimpleArray<VkDescriptorSet>	triangleVertexDescriptorSet;

	UniformBufferHandle matBufferHandle;
	StaticBufferHandle	vertexBufferHandle;


	void PrepareVulkanProps(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	void ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames);


	/* DirectX */

	/* Metal */


	/*===== END Graphics API =====*/
#pragma endregion

#pragma region SCENE INTERFACE 
public:
	/*===== Scene Interface =====*/

	//virtual ~RasterTriangle();

	virtual void Prepare(class GraphicsAPIManager& GAPI)final;

	virtual void Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)final;

	__forceinline virtual const char* Name()override { return "Raster Object"; }

	virtual void Act(struct AppWideContext& AppContext)final;

	virtual void Show(GAPIHandle& GAPIHandle)final;

	virtual void Close(class GraphicsAPIManager& GAPI)final;


	struct ObjectData
	{
		vec3 pos;
		vec3 euler_angles;
		vec3 scale;
	};
	ObjectData oData;

	struct UniformBuffer
	{
		mat4 model;
		mat4 view;
		mat4 proj;
	};
	UniformBuffer matBuffer;


	/*===== END Scene Interface =====*/
#pragma endregion 


};

#endif //__RASTER_OBJECT_H__