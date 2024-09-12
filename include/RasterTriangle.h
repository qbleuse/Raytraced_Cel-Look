#ifndef __RASTER_TRIANGLE_H__
#define __RASTER_TRIANGLE_H__


//include parent
#include "Scene.h"

//Graphics API
#include "ConcatenatedVulkan.h"

//for imgui and context
#include "AppWideContext.h"



/**
* This class is the beginning and first scene of all good Graphics Project : a Rasterized Triangle !
*/
class RasterTriangle : public Scene
{

	struct UniformBuffer
	{
		ImVec4 first;
		ImVec4 second;
		ImVec4 third;
		ImVec4 padding;
	};

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//the viewport of this scene (can change at runtime, as window can be resized)
	VkViewport				triangleViewport{};
	VkRect2D				triangleScissors{};
	VkRenderPass			triangleRenderPass{};
	VkDescriptorSetLayout	triangleVertexDescriptorLayout{};
	//VkDescriptorSetLayout	trianglePixelDescriptorLayout{};
	VkDescriptorPool		triangleDescriptorPool{};
	VkPipelineLayout		triangleLayout{};
	VkPipeline				trianglePipeline{};
	VkFramebuffer*			triangleOutput{nullptr};

	VkBuffer*				triangleVertexUniformBuffer{nullptr};
	VkDeviceMemory*			triangleVertexGPUUniformBuffer{ nullptr };
	void**					triangleVertexCPUUniformBuffer{ nullptr };
	VkDescriptorSet*		triangleVertexDescriptorSet{ nullptr };
	VkBuffer*				trianglePixelUniformBuffer{ nullptr };
	VkDeviceMemory*			trianglePixelGPUUniformBuffer{ nullptr };
	void**					trianglePixelCPUUniformBuffer{ nullptr };
	//VkDescriptorSet*		trianglePixelDescriptorSet{ nullptr };


	void PrepareVulkanProps(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	void ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height);


	/* DirectX */

	/* Metal */


	/*===== END Graphics API =====*/
#pragma endregion

#pragma region SCENE INTERFACE 
public:
	/*===== Scene Interface =====*/

	//virtual ~RasterTriangle();

	virtual void Prepare(class GraphicsAPIManager& GAPI)final;

	virtual void Resize(class GraphicsAPIManager& GAPI, int32_t width, int32_t height)final;

	virtual void Act(struct AppWideContext& AppContext)final;

	virtual void Show(class GraphicsAPIManager& GAPI)final;

	virtual void Close(class GraphicsAPIManager& GAPI)final;


	bool changed = true;

	UniformBuffer pointBuffer;
	UniformBuffer colorBuffer;



	/*===== END Scene Interface =====*/
#pragma endregion 


};

#endif //__RASTER_TRIANGLE_H__