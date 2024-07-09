#ifndef __RASTER_TRIANGLE_H__
#define __RASTER_TRIANGLE_H__


//include parent
#include "Scene.h"

//Graphics API
#include "ConcatenatedVulkan.h"

/**
* This class is the beginning and first scene of all good Graphics Project : a Rasterized Triangle !
*/
class RasterTriangle : public Scene
{

#pragma region GRAPHICS API
private:
	/*===== Graphics API =====*/

	/* Vulkan */

	//the viewport of this scene (can change at runtime, as window can be resized)
	VkViewport triangleViewport{};
	VkRenderPass triangleRenderPass{};
	VkPipeline trianglePipeline{};

	void PrepareVulkanProps(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);
	void PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader);

	/* DirectX */

	/* Metal */


	/*===== END Graphics API =====*/
#pragma endregion

#pragma region SCENE INTERFACE 
public:
	/*===== Scene Interface =====*/

	virtual ~RasterTriangle();

	virtual void Prepare(class GraphicsAPIManager& GAPI)final;

	virtual void Act(struct AppWideContext& AppContext)final;

	virtual void Show(class GraphicsAPIManager& GAPI)final;

	virtual void Close(class GraphicsAPIManager& GAPI)final;


	/*===== END Scene Interface =====*/
#pragma endregion 


};

#endif //__RASTER_TRIANGLE_H__