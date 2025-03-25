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


	CompileVulkanShaders(GAPI._VulkanUploader, VertexShader, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raster Triangle Vertex");
	CompileVulkanShaders(GAPI._VulkanUploader, FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raster Triangle Frag");

}

void RasterTriangle::PrepareVulkanProps(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{
	VkResult result = VK_SUCCESS;

	/*===== SHADER ======*/

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

	/*===== VERTEX INPUT ======*/

	//the vertex input for the input assembly stage. in this scene, there will not be any vertices, so we'll fill it up with nothing.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType							= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount	= 0;
	vertexInputInfo.pVertexBindingDescriptions		= nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions	= nullptr; // Optional

	//the description of the input assembly stage. by the fact we won't have any vertex, the input assembly will basically only give the indices (basically 0,1,2 if we ask for a draw command of three vertices)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType						= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology					= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable	= VK_FALSE;

	/*===== VIEWPORT/SCISSORS  =====*/

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
	viewportStateInfo.pViewports		= &_TriViewport;
	viewportStateInfo.scissorCount		= 1;
	viewportStateInfo.pScissors			= &_TriScissors;

	/*==== RASTERIZER =====*/

	//the description of our rasterizer : a very simple one, that will just fill up polygon (in this case one)
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable	= VK_FALSE;
	rasterizer.polygonMode		= VK_POLYGON_MODE_FILL;
	rasterizer.cullMode			= VK_CULL_MODE_NONE;
	rasterizer.frontFace		= VK_FRONT_FACE_CLOCKWISE;
	rasterizer.lineWidth		= 1.0f;

	/*===== MSAA =====*/

	//the description of the MSAA, here we just don't want it, an aliased _Tri is more than enough
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable	= VK_FALSE;
	multisampling.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	/*===== FRAMEBUFFER BLEND ======*/

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

	{
		/*===== FRAMEBUFFER OUTPUT ======*/

		//describing the format of the output (our framebuffers)
		VkAttachmentDescription frameColourBufferAttachment{};
		frameColourBufferAttachment.format			= GAPI._VulkanSurfaceFormat.format;
		frameColourBufferAttachment.samples			= VK_SAMPLE_COUNT_1_BIT;
		frameColourBufferAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;//we'll make the pipeline clear
		frameColourBufferAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;//we want to write into the framebuffer
		frameColourBufferAttachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;//we write onto the entire vewport so it will be completely replaced, what was before does not interests us
		frameColourBufferAttachment.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//there is no post process this is immediate writing into frame buffer

		/*===== SUBPASS DESCRIPTION ======*/

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
		renderPassInfo.pSubpasses = &subpass;

		VkResult result = VK_SUCCESS;
		VK_CALL_PRINT(vkCreateRenderPass(GAPI._VulkanDevice, &renderPassInfo, nullptr, &_TriRenderPass));
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{

		//creating our vertex uniform buffer
		VkDescriptorSetLayoutBinding vertexUniformLayoutBinding{};
		vertexUniformLayoutBinding.binding			= 0;
		vertexUniformLayoutBinding.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vertexUniformLayoutBinding.descriptorCount	= 1;
		vertexUniformLayoutBinding.stageFlags		= VK_SHADER_STAGE_VERTEX_BIT;

		//creating our pixel uniform buffer
		VkDescriptorSetLayoutBinding pixelUniformLayoutBinding{};
		pixelUniformLayoutBinding.binding			= 1;
		pixelUniformLayoutBinding.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pixelUniformLayoutBinding.descriptorCount	= 1;
		pixelUniformLayoutBinding.stageFlags		= VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 2;

		VkDescriptorSetLayoutBinding uniformLayoutBinding[2] = { vertexUniformLayoutBinding, pixelUniformLayoutBinding };
		layoutInfo.pBindings = uniformLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_TriVertexDescriptorLayout));

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts		= &_TriVertexDescriptorLayout;
		pipelineLayoutInfo.setLayoutCount	= 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_TriLayout));

	}

	/*===== PIPELINE ======*/

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
	pipelineInfo.layout					= _TriLayout;
	pipelineInfo.renderPass				= _TriRenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_TriPipeline));

	vkDestroyShaderModule(GAPI._VulkanDevice, FragmentShader, nullptr);
	vkDestroyShaderModule(GAPI._VulkanDevice, VertexShader, nullptr);
}


void RasterTriangle::Prepare(GraphicsAPIManager& GAPI)
{
	//the shaders needed
	VkShaderModule vertexShader{}, fragmentShader{};

	//fill up uniform buffers with initial data
	_PointBuffer.first	= vec4{0.0f, -1.0f, 0.0f, 0.0f};
	_PointBuffer.second	= vec4{0.5f, 0.5f, 0.0f, 0.0f};
	_PointBuffer.third	= vec4{-0.5f, 0.5f, 0.0f, 0.0f};

	_ColourBuffer.first = vec4{1.0f, 0.0f, 0.0f, 0.0f};
	_ColourBuffer.second = vec4{0.0f, 1.0f, 0.0f, 0.0f};
	_ColourBuffer.third = vec4{0.0f, 0.0f, 1.0f, 0.0f};


	//compile the shaders here
	PrepareVulkanScripts(GAPI, vertexShader, fragmentShader);

	//create the pipeline and copy commands here
	PrepareVulkanProps(GAPI, vertexShader, fragmentShader);

	enabled = true;
}

/*===== Resize =====*/

void RasterTriangle::ResizeVulkanResource(GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//first, we clear previously used resources
	VK_CLEAR_ARRAY(_TriOutput, old_nb_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _TriDescriptorPool, nullptr);
	_TriVertexDescriptorSet.Clear();

	/*===== VIEWPORT AND SCISSORS ======*/

	//filling up the viewport...
	_TriViewport.width = (float)GAPI._vk_width;
	_TriViewport.height = (float)GAPI._vk_height;
	_TriViewport.maxDepth = 1.0f;
	_TriViewport.minDepth = 0.0f;
	_TriViewport.x = 0.0f;
	_TriViewport.y = 0.0f;

	//... and the scissors
	_TriScissors.extent = { (uint32_t)GAPI._vk_width, (uint32_t)GAPI._vk_height };

	/*===== DESCRIPTORS ======*/

	{
		//describing how many descriptor at a time should be allocated
		VkDescriptorPoolSize poolSize{};
		poolSize.type				= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount	= 2 * GAPI._nb_vk_frames;//we need two : one for vertices, one for colours

		//creating our descriptor pool to allocate sets for each frame
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 1;
		poolInfo.pPoolSizes		= &poolSize;
		poolInfo.maxSets		= GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_TriDescriptorPool))

		//allocating a descriptor set for each of our framebuffer (a uniform buffer per frame)
		VkDescriptorSetAllocateInfo allocInfo {};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= _TriDescriptorPool;
		allocInfo.descriptorSetCount	= GAPI._nb_vk_frames;

		//copying the same layout for every descriptor set
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_TriVertexDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//then create the resource
		_TriVertexDescriptorSet.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_TriVertexDescriptorSet));
	}

	/*===== FRAMEBUFFERS ======*/

	//creating the description of the output of our pipeline
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType		= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass	= _TriRenderPass;
	framebufferInfo.width		= GAPI._vk_width;
	framebufferInfo.height		= GAPI._vk_height;
	framebufferInfo.layers		= 1;
	//allocating space for our outputs
	_TriOutput.Alloc(GAPI._nb_vk_frames);

	/*===== UNIFORM BUFFERS ======*/

	//recreate the uniform bufferx
	CreateUniformBufferHandle(GAPI._VulkanUploader, _TriPointsHandle, GAPI._nb_vk_frames, sizeof(UniformBuffer));
	CreateUniformBufferHandle(GAPI._VulkanUploader, _TriColourHandle, GAPI._nb_vk_frames, sizeof(UniformBuffer));

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//link our framebuffers (our pipeline's output) to the backbuffers
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &GAPI._VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI._VulkanDevice, &framebufferInfo, nullptr, &_TriOutput[i]))

		//describing our uniform vertices buffer
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = _TriPointsHandle._GPUBuffer[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBuffer);

		//then link it to the descriptor
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet			= _TriVertexDescriptorSet[i];
		descriptorWrite.dstBinding		= 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo		= &bufferInfo;

		//describing our uniform colour buffer
		VkDescriptorBufferInfo colorbufferInfo{};
		colorbufferInfo.buffer	= _TriColourHandle._GPUBuffer[i];
		colorbufferInfo.offset	= 0;
		colorbufferInfo.range	= sizeof(UniformBuffer);

		//then link it to the descriptor
		VkWriteDescriptorSet colordescriptorWrite{};
		colordescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colordescriptorWrite.dstSet				= _TriVertexDescriptorSet[i];
		colordescriptorWrite.dstBinding			= 1;
		colordescriptorWrite.dstArrayElement	= 0;
		colordescriptorWrite.descriptorType		= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		colordescriptorWrite.descriptorCount	= 1;
		colordescriptorWrite.pBufferInfo		= &colorbufferInfo;


		VkWriteDescriptorSet write[2] = { descriptorWrite, colordescriptorWrite };
		vkUpdateDescriptorSets(GAPI._VulkanDevice, 2, write, 0, nullptr);
	}


	_changed = true;
}


void RasterTriangle::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
}


/*===== Act =====*/

void RasterTriangle::Act(AppWideContext& AppContext)
{
	//drag and drop vertices
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		if (length(_PointBuffer.first.xy - AppContext.window_mouse_pos.xy) < 0.1f)
		{
			_PointBuffer.first.xy = AppContext.window_mouse_pos.xy;
			_changed = true;
		}
		if (length(_PointBuffer.second.xy - AppContext.window_mouse_pos.xy) < 0.1f)
		{
			_PointBuffer.second.xy = AppContext.window_mouse_pos.xy;
			_changed = true;
		}
		if (length(_PointBuffer.third.xy - AppContext.window_mouse_pos.xy) < 0.1f)
		{
			_PointBuffer.third.xy = AppContext.window_mouse_pos.xy;
			_changed = true;
		}
	}

	//UI update
	if (SceneCanShowUI(AppContext))
	{
		//change vertex position
		{
			_changed |= ImGui::SliderFloat2("First Vertex", _PointBuffer.first.xy.scalar, -1.0f, 1.0f);
			_changed |= ImGui::SliderFloat2("Second Vertex", _PointBuffer.second.xy.scalar, -1.0f, 1.0f);
			_changed |= ImGui::SliderFloat2("Third Vertex", _PointBuffer.third.xy.scalar, -1.0f, 1.0f);
		}

		//change vertex colors
		if (ImGui::CollapsingHeader("Colors"))
		{

			ImGui::BeginGroup();
			
			float width = (ImGui::CalcItemWidth() / 3.0f);
			ImGui::PushItemWidth(width);
			_changed |= ImGui::ColorPicker4("First", _ColourBuffer.first.scalar);
			ImGui::PopItemWidth();
			ImGui::SameLine(0, 0);
			ImGui::PushItemWidth(width);
			_changed |= ImGui::ColorPicker4("Second", _ColourBuffer.second.scalar);
			ImGui::PopItemWidth();
			ImGui::SameLine(0, 0);
			ImGui::PushItemWidth(width);
			_changed |= ImGui::ColorPicker4("Third", _ColourBuffer.third.scalar);
			ImGui::PopItemWidth();
			ImGui::EndGroup();
		}
	}
}

/*===== Show =====*/

void RasterTriangle::Show(GAPIHandle& RuntimeGAPIHandle)
{
	//to record errors
	VkResult result = VK_SUCCESS;

	//current frames resources
	VkCommandBuffer commandBuffer	= RuntimeGAPIHandle.GetCurrentVulkanCommand();
	VkSemaphore	waitSemaphore		= RuntimeGAPIHandle.GetCurrentCanPresentSemaphore();
	VkSemaphore	signalSemaphore		= RuntimeGAPIHandle.GetCurrentHasPresentedSemaphore();

	{
		//first, reset previous records
		VK_CALL_PRINT(vkResetCommandBuffer(commandBuffer, 0));
		
		//then open for record
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CALL_PRINT(vkBeginCommandBuffer(commandBuffer, &info));

	}
	{
		//Set output and output settings for this render pass.
		VkRenderPassBeginInfo info = {};
		info.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass					= _TriRenderPass;//obviously using this scene's renderpass
		info.framebuffer				= _TriOutput[RuntimeGAPIHandle._vk_frame_index];//writing in the available backbuffer
		info.renderArea.extent.width	= _TriViewport.width;//writing on the whole screen
		info.renderArea.extent.height	= _TriViewport.height;//writing on the whole screen
		info.clearValueCount			= 1;

		VkClearValue clearValue{};
		clearValue.color				= { 0.2f, 0.2f, 0.2f, 1.0f };
		info.pClearValues				= &clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//while the our uniform buffers are always bound,
	//and always check disparity between GPU buffer and CPU buffer's content,
	//we will only copy when something really changes (as this is still heavy)
	if (_changed)
	{
		for (uint32_t i = 0; i < RuntimeGAPIHandle._nb_vk_frames; i++)
		{
			memcpy(_TriPointsHandle._CPUMemoryHandle[i], (void*)&_PointBuffer, sizeof(UniformBuffer));
			memcpy(_TriColourHandle._CPUMemoryHandle[i], (void*)&_ColourBuffer, sizeof(UniformBuffer));
		}

		_changed = false;
	}

	//binding the pipeline to rasteriwe our triangle
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _TriPipeline);
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _TriLayout, 0, 1, &_TriVertexDescriptorSet[RuntimeGAPIHandle._vk_current_frame], 0, nullptr);

	//set viewport and scissors to draw on all screen
	vkCmdSetViewport(commandBuffer, 0, 1, &_TriViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_TriScissors);

	//finally call for draw (while we don't have a vertex buffer, 
	// we will only get indices, which is enough for us to show a triangle with our uniform buffer).
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	//end writing in our backbuffer
	vkCmdEndRenderPass(commandBuffer);
	{
		//the pipeline stage at which the GPU should waait for the semaphore to signal itself
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		
		//then we submit our commands, while setting the necesssary fences (semaphores on GPU), 
		//to schedule work properly
		VkSubmitInfo info = {};
		info.sType					= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount		= 1;
		info.pWaitSemaphores		= &waitSemaphore;//the semaphore we wait before beginning work
		info.pWaitDstStageMask		= &wait_stage;//the pipeline stage at which we will wait the semaphore
		info.commandBufferCount		= 1;
		info.pCommandBuffers		= &commandBuffer;//the recorded workload
		info.signalSemaphoreCount	= 1;
		info.pSignalSemaphores		= &signalSemaphore;//the semaphore to signal when finishing all the work

		//closing our command record ...
		VK_CALL_PRINT(vkEndCommandBuffer(commandBuffer));
		//... and submit it straight away
		VK_CALL_PRINT(vkQueueSubmit(RuntimeGAPIHandle._VulkanQueues[0], 1, &info, nullptr));
	}

}

/*===== Close =====*/

void RasterTriangle::Close(class GraphicsAPIManager& GAPI)
{
	//releasing the "per frames" objects
	VK_CLEAR_ARRAY(_TriOutput, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _TriPointsHandle);
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _TriColourHandle);

	//release descriptors
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _TriDescriptorPool, nullptr);
	_TriVertexDescriptorSet.Clear();
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _TriVertexDescriptorLayout, nullptr);

	//release pipeline objects
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _TriLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _TriPipeline, nullptr);
	vkDestroyRenderPass(GAPI._VulkanDevice, _TriRenderPass, nullptr);
}
