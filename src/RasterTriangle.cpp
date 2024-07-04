#include "RasterTriangle.h"

//include app class
#include "GraphicsAPIManager.h"
#include "AppWideContext.h"

//vulkan shader compiler
#include "shaderc/shaderc.h"

/*===== Prepare =====*/

void RasterTriangle::PrepareVulkanScripts(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{
	//define vertex shader
	const char* vertex_shader = 
		R"(#version 450
			
			vec2 positions[3] = vec2[](
			    vec2(0.0, -0.5),
			    vec2(0.5, 0.5),
			    vec2(-0.5, 0.5)
			);

			void main()
			{
				gl_Position = vec4(positions[gl_VertexIndex],  0.0, 1.0);
			})";

	//our shader compiler 
	shaderc_compiler_t shader_compiler = shaderc_compiler_initialize();

	//the compile option of our compiler
	shaderc_compile_options_t compile_options = shaderc_compile_options_initialize();
	
	//compiling our vertex shader into byte code
	shaderc_compilation_result_t compiled_vertex = shaderc_compile_into_spv(shader_compiler, vertex_shader, strlen(vertex_shader), shaderc_vertex_shader, "Raster Triangle Vertex", "main", compile_options);


	// make compiled shader, vulkan shaders
	{
		//the Vulkan Vertex Shader 
		VkShaderModuleCreateInfo shaderinfo{};
		shaderinfo.sType	= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderinfo.codeSize = shaderc_result_get_length(compiled_vertex);
		shaderinfo.pCode	= (uint32_t*)shaderc_result_get_bytes(compiled_vertex);

		VkResult result = VK_SUCCESS;
		VK_CALL_PRINT(vkCreateShaderModule(GAPI.VulkanDevice, &shaderinfo, nullptr, &VertexShader))
	}



	shaderc_result_release(compiled_vertex);

	//release both compile options and compiler
	shaderc_compile_options_release(compile_options);
	shaderc_compiler_release(shader_compiler);
}

void RasterTriangle::PrepareVulkanProps(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{
	//describe vertex shader stage 
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = VertexShader;
	vertShaderStageInfo.pName  = "main";

	

	vkDestroyShaderModule(GAPI.VulkanDevice, VertexShader, nullptr);
}


void RasterTriangle::Prepare(GraphicsAPIManager& GAPI)
{
	//the Shaders needed.
	VkShaderModule vertexShader{}, fragmentShader{};

	//compile the shaders here
	PrepareVulkanScripts(GAPI, vertexShader, fragmentShader);

	//create the pipeline and draw commands here
	PrepareVulkanProps(GAPI, vertexShader, fragmentShader);
}

/*===== Act =====*/

void RasterTriangle::Act(AppWideContext& AppContext)
{

}

/*===== Show =====*/

void RasterTriangle::Show(GraphicsAPIManager& GAPI)
{

}

/*===== Close =====*/

void RasterTriangle::Close(class GraphicsAPIManager& GAPI)
{

}
