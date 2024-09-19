#include "RasterObject.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"




/*==== Prepare =====*/

void RasterObject::PrepareVulkanProps(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{

}

void RasterObject::PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{

}

void RasterObject::Prepare(class GraphicsAPIManager& GAPI)
{
	VkShaderModule VertexShader, FragmentShader;

	PrepareVulkanScripts(GAPI, VertexShader, FragmentShader);
	PrepareVulkanProps(GAPI, VertexShader, FragmentShader);
}

/*===== Resize =====*/

void RasterObject::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height)
{

}


void RasterObject::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height);
}

/*===== Act =====*/

void RasterObject::Act(struct AppWideContext& AppContext)
{

}

/*===== Show =====*/

void RasterObject::Show(GAPIHandle& GAPIHandle)
{

}

/*===== Close =====*/

void RasterObject::Close(class GraphicsAPIManager& GAPI)
{

}
