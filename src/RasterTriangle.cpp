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

	const char* fragment_shader =
		R"(#version 450
		layout(location = 0) out vec4 outColor;

		void main()
		{
			outColor = vec4(1.0,1.0,1.0,1.0);
		}
		)";

	//our shader compiler 
	shaderc_compiler_t shader_compiler = shaderc_compiler_initialize();

	//the compile option of our compiler
	shaderc_compile_options_t compile_options = shaderc_compile_options_initialize();
	
	//compiling our vertex shader into byte code
	shaderc_compilation_result_t compiled_vertex = shaderc_compile_into_spv(shader_compiler, vertex_shader, strlen(vertex_shader), shaderc_vertex_shader, "Raster Triangle Vertex", "main", compile_options);
	//compiling our fragment shader into byte code
	shaderc_compilation_result_t compiled_fragment = shaderc_compile_into_spv(shader_compiler, fragment_shader, strlen(fragment_shader), shaderc_vertex_shader, "Raster Triangle Fragment", "main", compile_options);


	// make compiled shader, vulkan shaders
	{
		//the Vulkan Vertex Shader 
		VkShaderModuleCreateInfo shaderinfo{};
		shaderinfo.sType	= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shaderinfo.codeSize = shaderc_result_get_length(compiled_vertex);
		shaderinfo.pCode	= (uint32_t*)shaderc_result_get_bytes(compiled_vertex);

		VkResult result = VK_SUCCESS;
		VK_CALL_PRINT(vkCreateShaderModule(GAPI.VulkanDevice, &shaderinfo, nullptr, &VertexShader))

		//the Vulkan Fragment Shader 
		shaderinfo.codeSize = shaderc_result_get_length(compiled_fragment);
		shaderinfo.pCode = (uint32_t*)shaderc_result_get_bytes(compiled_fragment);

		VK_CALL_PRINT(vkCreateShaderModule(GAPI.VulkanDevice, &shaderinfo, nullptr, &FragmentShader))
	}


	shaderc_result_release(compiled_vertex);
	shaderc_result_release(compiled_fragment);


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

	//describe vertex shader stage 
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	vertShaderStageInfo.module = FragmentShader;
	vertShaderStageInfo.pName = "main";

	//the vertex input for the input assembly stage. in this scene, there will not be any vertices, so we'll fill it up with nothing.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType							= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount	= 0;
	vertexInputInfo.pVertexBindingDescriptions		= nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions	= nullptr; // Optional

	//the description of the input assembly stage. by the fact we won't have any vertex, the input assembly will basically only give the indices (basically 0,1,2 if we ask for a draw command of three vertices)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType					= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology				= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	//most things are described in advance in vulkan pipeline, but this is the config of thing that can be changed on the fly. in our situation it will be only Viewport.
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount	= 1;
	VkDynamicState viewportState		= VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStateInfo.pDynamicStates		= &viewportState;

	//describing our viewport (even it is still dynamic)
	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount		= 1;
	viewportStateInfo.pViewports		= &triangleViewport;
	viewportStateInfo.scissorCount		= 0;
	viewportStateInfo.pScissors			= nullptr;

	//the description of our rasterizer : a very simple one, that will just fill up polygon (in this case one)
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable	= VK_FALSE;
	rasterizer.polygonMode		= VK_POLYGON_MODE_FILL;
	rasterizer.cullMode			= VK_CULL_MODE_NONE;
	rasterizer.frontFace		= VK_FRONT_FACE_CLOCKWISE;

	//the description of the MSAA, here we just don't want it, an aliased triangle is more than enough
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable	= VK_FALSE;
	multisampling.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	//description of colour bkending and writing configuration : here we write every component of the output, but don;t do alpha blending or other stuff
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask	= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable	= VK_FALSE;

	//description of colour blending :  here it is a simple write and discard old.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable		= VK_FALSE;
	colorBlending.attachmentCount	= 1;
	colorBlending.pAttachments		= &colorBlendAttachment;

	//describing the format of the output (our framebuffers)
	VkAttachmentDescription frameColourBufferAttachment{};
	frameColourBufferAttachment.format			= GAPI.VulkanSurfaceFormat.format;
	frameColourBufferAttachment.samples			= VK_SAMPLE_COUNT_1_BIT;
	frameColourBufferAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;//we won't let pipeline clear, this should be application wide stuff
	frameColourBufferAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;//we want to write into the framebuffer
	frameColourBufferAttachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;//we write onto the entire vewport so it will be completely replaced, what was before does not interests us
	frameColourBufferAttachment.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//there is no post process this is immediate writing into frame buffer

	//describing the output in the subpass
	VkAttachmentReference frameColourBufferAttachmentRef{};
	frameColourBufferAttachmentRef.attachment	= 0;
	frameColourBufferAttachmentRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//describing the subpass of our main renderpass
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &frameColourBufferAttachmentRef;

	//describing our renderpass 
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount	= 1;
	renderPassInfo.pAttachments		= &frameColourBufferAttachment;
	renderPassInfo.subpassCount		= 1;
	renderPassInfo.pSubpasses		= &subpass;

	VkResult result = VK_SUCCESS;
	VK_CALL_PRINT(vkCreateRenderPass(GAPI.VulkanDevice, &renderPassInfo, nullptr, &triangleRenderPass));



	
	vkDestroyShaderModule(GAPI.VulkanDevice, FragmentShader, nullptr);
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
	vkDestroyPipeline(GAPI.VulkanDevice, trianglePipeline, nullptr);
	vkDestroyRenderPass(GAPI.VulkanDevice, triangleRenderPass, nullptr);
}
