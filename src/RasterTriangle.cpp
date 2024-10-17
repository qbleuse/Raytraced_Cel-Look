#include "RasterTriangle.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"
#include "imgui/imgui.h"


/*===== Prepare =====*/

void RasterTriangle::PrepareVulkanScripts(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{
	//define vertex shader
	const char* vertex_shader =
		R"(#version 450

			layout(binding = 0) uniform PointBufferObject
			{
				vec4 points[4];
			} point_ubo;

			layout(binding = 1) uniform ColorBufferObject
			{
				vec4 points[4];
			} color_ubo;

			layout(location = 0) out vec3 fragColor;

			void main()
			{
				gl_Position = vec4(point_ubo.points[gl_VertexIndex].xy,  0.0, 1.0);
				fragColor = color_ubo.points[gl_VertexIndex].rgb;

			})";

	const char* fragment_shader =
		R"(#version 450
		layout(location = 0) in vec3 fragColor;
		layout(location = 0) out vec4 outColor;

		void main()
		{
			outColor = vec4(fragColor,1.0);
		}
		)";


	CreateVulkanShaders(GAPI.VulkanUploader, VertexShader, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raster Triangle Vertex");
	CreateVulkanShaders(GAPI.VulkanUploader, FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raster Triangle Frag");

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
	frameColourBufferAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;//we'll make the pipeline clear
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
	pixelUniformLayoutBinding.binding = 1;
	pixelUniformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pixelUniformLayoutBinding.descriptorCount = 1;
	pixelUniformLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	VkDescriptorSetLayoutBinding uniformLayoutBinding[2] = { vertexUniformLayoutBinding, pixelUniformLayoutBinding };
	layoutInfo.pBindings = uniformLayoutBinding;

	VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI.VulkanDevice, &layoutInfo, nullptr, &triangleVertexDescriptorLayout));

	//layoutInfo.pBindings = &pixelUniformLayoutBinding;
	//VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI.VulkanDevice, &layoutInfo, nullptr, &trianglePixelDescriptorLayout));

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	//VkDescriptorSetLayout uniformLayout[2] = { triangleVertexDescriptorLayout, trianglePixelDescriptorLayout };
	pipelineLayoutInfo.pSetLayouts = &triangleVertexDescriptorLayout;
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

	pointBuffer.first	= vec4{0.0f, -1.0f, 0.0f, 0.0f};
	pointBuffer.second	= vec4{0.5f, 0.5f, 0.0f, 0.0f};
	pointBuffer.third	= vec4{-0.5f, 0.5f, 0.0f, 0.0f};

	colorBuffer.first = vec4{1.0f, 0.0f, 0.0f, 0.0f};
	colorBuffer.second = vec4{0.0f, 1.0f, 0.0f, 0.0f};
	colorBuffer.third = vec4{0.0f, 0.0f, 1.0f, 0.0f};


	//compile the shaders here
	PrepareVulkanScripts(GAPI, vertexShader, fragmentShader);

	//create the pipeline and draw commands here
	PrepareVulkanProps(GAPI, vertexShader, fragmentShader);
}

/*===== Resize =====*/

void RasterTriangle::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	VK_CLEAR_ARRAY(triangleOutput, old_nb_frames, vkDestroyFramebuffer, GAPI.VulkanDevice);
	vkDestroyDescriptorPool(GAPI.VulkanDevice, triangleDescriptorPool, nullptr);


	//creating the description of the output of our pipeline
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass		= triangleRenderPass;
	framebufferInfo.width			= GAPI.VulkanWidth;
	framebufferInfo.height			= GAPI.VulkanHeight;
	framebufferInfo.layers = 1;

	//filling up the viewport
	triangleViewport.width = (float)GAPI.VulkanWidth;
	triangleViewport.height = (float)GAPI.VulkanHeight;
	triangleViewport.maxDepth = 1.0f;
	triangleViewport.minDepth = 0.0f;
	triangleViewport.x = 0.0f;
	triangleViewport.y = 0.0f;

	triangleScissors.extent = { (uint32_t)GAPI.VulkanWidth, (uint32_t)GAPI.VulkanHeight};

	triangleOutput.Alloc(GAPI.NbVulkanFrames);

	//create the descriptor pool
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = GAPI.NbVulkanFrames;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 2;
	VkDescriptorPoolSize doublePoolSize[2] = {poolSize, poolSize};
	poolInfo.pPoolSizes = doublePoolSize;
	poolInfo.maxSets = GAPI.NbVulkanFrames;

	VK_CALL_PRINT(vkCreateDescriptorPool(GAPI.VulkanDevice, &poolInfo, nullptr, &triangleDescriptorPool))

	//describe a descriptor set for each of our framebuffer (a uniform bufer per frame)
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = triangleDescriptorPool;
	allocInfo.descriptorSetCount = GAPI.NbVulkanFrames;
	VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI.NbVulkanFrames * sizeof(VkDescriptorSetLayout));
	for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
		memcpy(&layouts[i], &triangleVertexDescriptorLayout, sizeof(VkDescriptorSetLayout));
	allocInfo.pSetLayouts = layouts;

	triangleVertexDescriptorSet.Alloc(GAPI.NbVulkanFrames);
	//trianglePixelDescriptorSet = (VkDescriptorSet*)malloc(sizeof(VkDescriptorSet) * GAPI.NbVulkanFrames);
	VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI.VulkanDevice, &allocInfo, *triangleVertexDescriptorSet));

	//recreate the uniform bufferx
	CreateUniformBufferHandle(GAPI.VulkanUploader, trianglePointsHandle, GAPI.NbVulkanFrames, sizeof(UniformBuffer));
	CreateUniformBufferHandle(GAPI.VulkanUploader, triangleColourHandle, GAPI.NbVulkanFrames, sizeof(UniformBuffer));



	for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
	{
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &GAPI.VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI.VulkanDevice, &framebufferInfo, nullptr, &triangleOutput[i]))


		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = trianglePointsHandle.GPUBuffer[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBuffer);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet			= triangleVertexDescriptorSet[i];
		descriptorWrite.dstBinding		= 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo		= &bufferInfo;

		VkDescriptorBufferInfo colorbufferInfo{};
		colorbufferInfo.buffer = triangleColourHandle.GPUBuffer[i];
		colorbufferInfo.offset = 0;
		colorbufferInfo.range = sizeof(UniformBuffer);

		VkWriteDescriptorSet colordescriptorWrite{};
		colordescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colordescriptorWrite.dstSet = triangleVertexDescriptorSet[i];
		colordescriptorWrite.dstBinding = 1;
		colordescriptorWrite.dstArrayElement = 0;
		colordescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		colordescriptorWrite.descriptorCount = 1;
		colordescriptorWrite.pBufferInfo = &colorbufferInfo;


		VkWriteDescriptorSet write[2] = { descriptorWrite, colordescriptorWrite };
		vkUpdateDescriptorSets(GAPI.VulkanDevice, 2, write, 0, nullptr);
	}


	changed = true;
}


void RasterTriangle::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
}


/*===== Act =====*/

void RasterTriangle::Act(AppWideContext& AppContext)
{

	if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		if (length(pointBuffer.first.xy - AppContext.window_mouse_pos.xy) < 0.1f)
		{
			pointBuffer.first.xy = AppContext.window_mouse_pos.xy;
			changed = true;
		}
		if (length(pointBuffer.second.xy - AppContext.window_mouse_pos.xy) < 0.1f)
		{
			pointBuffer.second.xy = AppContext.window_mouse_pos.xy;
			changed = true;
		}
		if (length(pointBuffer.third.xy - AppContext.window_mouse_pos.xy) < 0.1f)
		{
			pointBuffer.third.xy = AppContext.window_mouse_pos.xy;
			changed = true;
		}
	}

	//UI update
	if (SceneCanShowUI(AppContext))
	{
		changed |= ImGui::SliderFloat2("First Point", pointBuffer.first.xy.scalar, -1.0f, 1.0f);
		changed |= ImGui::SliderFloat2("Second Point", pointBuffer.second.xy.scalar, -1.0f, 1.0f);
		changed |= ImGui::SliderFloat2("Third Point", pointBuffer.third.xy.scalar, -1.0f, 1.0f);

		changed |= ImGui::ColorPicker4("First Point Color", colorBuffer.first.scalar);
		changed |= ImGui::ColorPicker4("Second Point Color", colorBuffer.second.scalar);
		changed |= ImGui::ColorPicker4("Third Point Color", colorBuffer.third.scalar);
	}
}

/*===== Show =====*/

void RasterTriangle::Show(GAPIHandle& RuntimeGAPIHandle)
{
	VkResult err;
	VkCommandBuffer commandBuffer = RuntimeGAPIHandle.GetCurrentVulkanCommand();
	VkSemaphore waitSemaphore = RuntimeGAPIHandle.GetCurrentCanPresentSemaphore();
	VkSemaphore signalSemaphore = RuntimeGAPIHandle.GetCurrentHasPresentedSemaphore();

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
		info.framebuffer	= triangleOutput[RuntimeGAPIHandle.VulkanFrameIndex];
		info.renderArea.extent.width = triangleViewport.width;
		info.renderArea.extent.height = triangleViewport.height;
		info.clearValueCount = 1;
		VkClearValue clearValue{};
		clearValue.color = { 0.2f, 0.2f, 0.2f, 1.0f };
		info.pClearValues = &clearValue;
		//info.pClearValues				= &imgui.ImGuiWindow.ClearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	if (changed)
	{
		for (uint32_t i = 0; i < RuntimeGAPIHandle.NbVulkanFrames; i++)
		{
			memcpy(trianglePointsHandle.CPUMemoryHandle[i], (void*)&pointBuffer, sizeof(UniformBuffer));
			memcpy(triangleColourHandle.CPUMemoryHandle[i], (void*)&colorBuffer, sizeof(UniformBuffer));
		}

		changed = false;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, triangleLayout, 0, 1, &triangleVertexDescriptorSet[RuntimeGAPIHandle.VulkanCurrentFrame], 0, nullptr);


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
		err = vkQueueSubmit(RuntimeGAPIHandle.VulkanQueues[0], 1, &info, nullptr);
		//check_vk_result(err);
	}

}

/*===== Close =====*/

void RasterTriangle::Close(class GraphicsAPIManager& GAPI)
{
	VK_CLEAR_ARRAY(triangleOutput, GAPI.NbVulkanFrames, vkDestroyFramebuffer, GAPI.VulkanDevice);

	VulkanHelper::ClearUniformBufferHandle(GAPI.VulkanDevice, trianglePointsHandle);
	VulkanHelper::ClearUniformBufferHandle(GAPI.VulkanDevice, triangleColourHandle);

	//vkFreeDescriptorSets(GAPI.VulkanDevice, triangleDescriptorPool, GAPI.NbVulkanFrames, triangleVertexDescriptorSet);
	//free(triangleVertexDescriptorSet);
	//vkFreeDescriptorSets(GAPI.VulkanDevice, triangleDescriptorPool, GAPI.NbVulkanFrames, trianglePixelDescriptorSet);
	//free(trianglePixelDescriptorSet);
	vkDestroyDescriptorPool(GAPI.VulkanDevice, triangleDescriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(GAPI.VulkanDevice, triangleVertexDescriptorLayout, nullptr);
	//vkDestroyDescriptorSetLayout(GAPI.VulkanDevice, trianglePixelDescriptorLayout, nullptr);
	vkDestroyPipelineLayout(GAPI.VulkanDevice, triangleLayout, nullptr);
	vkDestroyPipeline(GAPI.VulkanDevice, trianglePipeline, nullptr);
	vkDestroyRenderPass(GAPI.VulkanDevice, triangleRenderPass, nullptr);
}
