#include "RasterTriangle.h"

//include app class
#include "GraphicsAPIManager.h"
#include "AppWideContext.h"
#include "VulkanHelper.h"

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
	shaderc_compilation_result_t compiled_fragment = shaderc_compile_into_spv(shader_compiler, fragment_shader, strlen(fragment_shader), shaderc_fragment_shader, "Raster Triangle Fragment", "main", compile_options);


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
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = FragmentShader;
	fragShaderStageInfo.pName = "main";

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
	dynamicStateInfo.dynamicStateCount	= 2;
	VkDynamicState viewportState[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	dynamicStateInfo.pDynamicStates		= viewportState;

	//describing our viewport (even it is still dynamic)
	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount		= 1;
	viewportStateInfo.pViewports		= &triangleViewport;
	viewportStateInfo.scissorCount		= 1;
	viewportStateInfo.pScissors			= &triangleScissors;

	//the description of our rasterizer : a very simple one, that will just fill up polygon (in this case one)
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable	= VK_FALSE;
	rasterizer.polygonMode		= VK_POLYGON_MODE_FILL;
	rasterizer.cullMode			= VK_CULL_MODE_NONE;
	rasterizer.frontFace		= VK_FRONT_FACE_CLOCKWISE;
	rasterizer.lineWidth		= 1.0f;

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

	//creating our uniform buffer
	VkDescriptorSetLayoutBinding vertexUniformLayoutBinding{};
	vertexUniformLayoutBinding.binding = 0;
	vertexUniformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vertexUniformLayoutBinding.descriptorCount = 1;
	vertexUniformLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding pixelUniformLayoutBinding{};
	pixelUniformLayoutBinding.binding = 0;
	pixelUniformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pixelUniformLayoutBinding.descriptorCount = 1;
	pixelUniformLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	VkDescriptorSetLayoutBinding uniformLayoutBinding[2] = { vertexUniformLayoutBinding, pixelUniformLayoutBinding };
	layoutInfo.pBindings = uniformLayoutBinding;

	VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI.VulkanDevice, &layoutInfo, nullptr, &triangleDescriptorLayout));

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pSetLayouts = &triangleDescriptorLayout;
	pipelineLayoutInfo.setLayoutCount = 1;

	VK_CALL_PRINT(vkCreatePipelineLayout(GAPI.VulkanDevice, &pipelineLayoutInfo, nullptr, &triangleLayout));

	//finally creating our pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//putting our two shader stages 
	VkPipelineShaderStageCreateInfo stages[2] = {vertShaderStageInfo, fragShaderStageInfo};
	pipelineInfo.pStages	= stages;
	pipelineInfo.stageCount = 2;

	//fill up the pipeline description
	pipelineInfo.pVertexInputState		= &vertexInputInfo;
	pipelineInfo.pInputAssemblyState	= &inputAssembly;
	pipelineInfo.pViewportState			= &viewportStateInfo;
	pipelineInfo.pRasterizationState	= &rasterizer;
	pipelineInfo.pMultisampleState		= &multisampling;
	pipelineInfo.pDepthStencilState		= nullptr; // Optional
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= &dynamicStateInfo;
	pipelineInfo.layout					= triangleLayout;
	pipelineInfo.renderPass				= triangleRenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI.VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &trianglePipeline));
	
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

/*===== Resize =====*/

void RasterTriangle::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height)
{
	VkResult result = VK_SUCCESS;

	VK_CLEAR_RAW_ARRAY(triangleOutput, GAPI.NbVulkanFrames, vkDestroyFramebuffer, GAPI.VulkanDevice);
	VK_CLEAR_RAW_ARRAY(triangleUniformBuffer, GAPI.NbVulkanFrames, vkDestroyBuffer, GAPI.VulkanDevice);
	VK_CLEAR_RAW_ARRAY(triangleGPUUniformBuffer, GAPI.NbVulkanFrames, vkFreeMemory, GAPI.VulkanDevice);
	free(triangleCPUUniformBuffer);


	//creating the description of the output of our pipeline
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass		= triangleRenderPass;
	framebufferInfo.width			= width;
	framebufferInfo.height			= height;
	framebufferInfo.layers = 1;

	//filling up the viewport
	triangleViewport.width = (float)width;
	triangleViewport.height = (float)height;
	triangleViewport.maxDepth = 1.0f;
	triangleViewport.minDepth = 0.0f;
	triangleViewport.x = 0.0f;
	triangleViewport.y = 0.0f;

	triangleScissors.extent = { (uint32_t)width, (uint32_t)height };

	triangleOutput = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * GAPI.NbVulkanFrames);

	triangleUniformBuffer = (VkBuffer*)malloc(sizeof(VkBuffer) * GAPI.NbVulkanFrames);
	triangleGPUUniformBuffer = (VkDeviceMemory*)malloc(sizeof(VkDeviceMemory) * GAPI.NbVulkanFrames);
	triangleCPUUniformBuffer = (void**)malloc(sizeof(void*) * GAPI.NbVulkanFrames);

	for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
	{
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &GAPI.VulkanBackColourBuffers[i];

		VK_CALL_PRINT(vkCreateFramebuffer(GAPI.VulkanDevice, &framebufferInfo, nullptr, &triangleOutput[i]))

		if (CreateVulkanBuffer(GAPI, sizeof(UniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, triangleUniformBuffer[i], triangleGPUUniformBuffer[i]))
		{
			VK_CALL_PRINT(vkMapMemory(GAPI.VulkanDevice, triangleGPUUniformBuffer[i], 0, sizeof(UniformBuffer), 0, &triangleCPUUniformBuffer[i]));
		}
	}
}


void RasterTriangle::Resize(class GraphicsAPIManager& GAPI, int32_t width, int32_t height)
{
	ResizeVulkanResource(GAPI, width, height);
}


/*===== Act =====*/

void RasterTriangle::Act(AppWideContext& AppContext)
{

}

/*===== Show =====*/

void RasterTriangle::Show(GraphicsAPIManager& GAPI)
{
	VkResult err;
	VkCommandBuffer commandBuffer = GAPI.GetCurrentVulkanCommand();
	VkSemaphore waitSemaphore = GAPI.GetCurrentCanPresentSemaphore();
	VkSemaphore signalSemaphore = GAPI.GetCurrentHasPresentedSemaphore();

	{
		err = vkResetCommandBuffer(commandBuffer, 0);
		//check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(commandBuffer, &info);
		//	check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass		= triangleRenderPass;
		info.framebuffer	= triangleOutput[GAPI.VulkanFrameIndex];
		info.renderArea.extent.width = triangleViewport.width;
		info.renderArea.extent.height = triangleViewport.height;
		info.clearValueCount = 0;
		//info.pClearValues				= &imgui.ImGuiWindow.ClearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &triangleViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &triangleScissors);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	// Submit command buffer
	vkCmdEndRenderPass(commandBuffer);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &waitSemaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &commandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &signalSemaphore;

		err = vkEndCommandBuffer(commandBuffer);
		//check_vk_result(err);
		err = vkQueueSubmit(GAPI.VulkanQueues[0], 1, &info, GAPI.VulkanIsDrawingFence[GAPI.VulkanFrameIndex]);
		//check_vk_result(err);
	}

}

/*===== Close =====*/

void RasterTriangle::Close(class GraphicsAPIManager& GAPI)
{
	VK_CLEAR_RAW_ARRAY(triangleOutput, GAPI.NbVulkanFrames, vkDestroyFramebuffer, GAPI.VulkanDevice);
	VK_CLEAR_RAW_ARRAY(triangleUniformBuffer, GAPI.NbVulkanFrames, vkDestroyBuffer, GAPI.VulkanDevice);
	VK_CLEAR_RAW_ARRAY(triangleGPUUniformBuffer, GAPI.NbVulkanFrames, vkFreeMemory, GAPI.VulkanDevice);
	free(triangleCPUUniformBuffer);

	vkDestroyDescriptorSetLayout(GAPI.VulkanDevice, triangleDescriptorLayout, nullptr);
	vkDestroyPipelineLayout(GAPI.VulkanDevice, triangleLayout, nullptr);
	vkDestroyPipeline(GAPI.VulkanDevice, trianglePipeline, nullptr);
	vkDestroyRenderPass(GAPI.VulkanDevice, triangleRenderPass, nullptr);
}
