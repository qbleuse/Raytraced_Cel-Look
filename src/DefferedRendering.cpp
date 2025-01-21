#include "DefferedRendering.h"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"

/*==== Prepare =====*/

void DefferedRendering::PrepareVulkanProps(GraphicsAPIManager& GAPI)
{
	VkResult result = VK_SUCCESS;

	{
		/*===== GBuffer OUTPUT ======*/
	
		//describing the format of the output (our framebuffers)
		VkAttachmentDescription attachements[3] =
		{
			//flags, format, samples, loadOp, storeOp, stencilLoadOp, stencilStoreOp, inialLayout, finalLayout
			{0, VK_FORMAT_R16G16B16A16_SFLOAT,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			{0, VK_FORMAT_R16G16B16A16_SFLOAT,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
			{0, VK_FORMAT_R16G16B16A16_SFLOAT,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
		};

		VulkanHelper::CreatePipelineOutput(GAPI._VulkanUploader, _DefferedPipelineOutput, attachements, 3, {});
	}
	{
		/*===== Deffered OUTPUT ======*/

		//describing the format of the output (our framebuffers)
		VkAttachmentDescription attachements[1] =
		{
			//flags, format, samples, loadOp, storeOp, stencilLoadOp, stencilStoreOp, inialLayout, finalLayout
			{0, GAPI._VulkanSurfaceFormat.format,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}
		};

		VulkanHelper::CreatePipelineOutput(GAPI._VulkanUploader, _DefferedPipelineOutput, attachements, 1, {});
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		/*===== GPU Buffer INPUT ======*/

		//the sampler and image to sample texture
		VkDescriptorSetLayoutBinding layoutBindings[1] =
		{
			{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
		};

		VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _GBufferDescriptors, layoutBindings, 1);


		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType		= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts	= &_GBufferDescriptors._DescriptorLayout;
		pipelineLayoutInfo.setLayoutCount = 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_GBUfferLayout));
	}

	{
		/*===== Deffered INPUT ======*/

		// a very basic sampler will be fine
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		vkCreateSampler(GAPI._VulkanDevice, &samplerInfo, nullptr, &_DefferedSampler);

		//the GPU Buffer images
		VkDescriptorSetLayoutBinding layoutBindings[4] =
		{
			{0, VK_DESCRIPTOR_TYPE_SAMPLER , 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_DefferedSampler},
			{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
			{2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
			{3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
		};

		VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, layoutBindings, 4);

		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts		= &_DefferedDescriptors._DescriptorLayout;
		pipelineLayoutInfo.setLayoutCount	= 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_DefferedLayout));
	}


	CreateFullscreenCopyPipeline(GAPI, _GBufferPipeline, _GBUfferLayout, _GBUfferPipelineOutput._OutputRenderPass, _GBufferShaders);
	CreateFullscreenCopyPipeline(GAPI, _DefferedPipeline, _DefferedLayout, _DefferedPipelineOutput._OutputRenderPass, _DefferedShaders);

}

void DefferedRendering::CreateFullscreenCopyPipeline(class GraphicsAPIManager& GAPI, VkPipeline& Pipeline, const VkPipelineLayout& PipelineLayout, const VulkanHelper::PipelineOutput& PipelineOutput, const List<VulkanHelper::ShaderScripts>& Shaders)
{
	VkResult result = VK_SUCCESS;

	/*===== SHADER ======*/

	//the shader stages we'll give to the pipeline
	MultipleScopedMemory<VkPipelineShaderStageCreateInfo> shaderStages;
	VulkanHelper::MakePipelineShaderFromScripts(shaderStages, Shaders);

	/*===== INTPUT ASSEMBLY ======*/

	//to do a full screen, you actually do not need any vertex
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	//the description of the input assembly stage. by the fact we won't have any vertex, the input assembly will basically only give the indices (basically 0,1,2 if we ask for a draw command of three vertices)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	/*===== VIEWPORT/SCISSORS  =====*/

	//as window can be resized, I set viewport and scissors as dynamic so it is always the size of the output window
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = 2;
	VkDynamicState viewportState[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	dynamicStateInfo.pDynamicStates = viewportState;

	//setting our starting viewport
	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = &PipelineOutput._OutputViewport;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = &PipelineOutput._OutputScissor;


	/*==== RASTERIZER =====*/

	//Here, our rasterizer actually does the conversion from the vertex created inthe vertex shader and pixel coordinates\
	//it is the same as always, and frankly does not do much 
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.lineWidth = 1.0f;

	/*===== MSAA =====*/

	//MSAA would be way overkill for what wee are doing, so just disabling it
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	/*===== FRAMEBUFFER BLEND ======*/

	//we'll write the texture as-is
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	//nothing crazy in colour blending, as we just want to show our CPU texture
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	/*===== PIPELINE ======*/

	//finally creating our pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//fill up the pipeline description
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.pStages = *shaderStages;
	pipelineInfo.stageCount = Shaders.Nb();
	pipelineInfo.layout = PipelineLayout;
	pipelineInfo.renderPass = PipelineOutput._OutputRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &Pipeline));
}

void DefferedRendering::PrepareVulkanScripts(GraphicsAPIManager& GAPI)
{
	//define vertex shader
	const char* vertex_shader =
		R"(#version 450
			layout(location = 0) out vec2 uv;

			void main()
			{
				uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);				
				gl_Position =  vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);

			})";

	const char* fragment_shader =
		R"(#version 450
		layout(location = 0) in vec2 uv;
		layout(location = 0) out vec4 outColor;

		layout(set = 0, binding = 0) uniform sampler2D fullscreenTex;


		void main()
		{
			outColor = vec4(texture(fullscreenTex,uv).rgb,1.0);
		}
		)";


	{
		VulkanHelper::ShaderScripts Script;

		//add vertex shader
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raytrace Fullscreen Vertex"))
			_CopyShaders.Add(Script);

		//add fragment shader
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raytrace Fullscreen Frag"))
			_CopyShaders.Add(Script);
	}
}

void DefferedRendering::Prepare(class GraphicsAPIManager& GAPI)
{
	//a "zero init" of the transform values
	_ObjData.scale = vec3{ 1.0f, 1.0f, 1.0f };
	_ObjData.pos = vec3{ 0.0f, 0.0f, 0.0f };

	//preparing full screen copy pipeline
	{
		//the shaders needed
		//compile the shaders here...
		PrepareVulkanScripts(GAPI);
		//then create the pipeline
		PrepareVulkanProps(GAPI);
	}

	enabled = true;
}

/*===== Resize =====*/

void DefferedRendering::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{

	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//clear the fullscreen images buffer
	ClearFrameBuffer(GAPI._VulkanDevice, _GBuffers[0]);
	ClearFrameBuffer(GAPI._VulkanDevice, _GBuffers[1]);
	ClearFrameBuffer(GAPI._VulkanDevice, _GBuffers[2]);
	_GBuffers.Clear();

	VulkanHelper::ReleaseFrameBuffer(GAPI._VulkanDevice, _GBUfferPipelineOutput);
	VulkanHelper::ReleaseFrameBuffer(GAPI._VulkanDevice, _DefferedPipelineOutput);
	VulkanHelper::ReleaseDescriptor(GAPI._VulkanDevice, _GBufferDescriptors);
	VulkanHelper::ReleaseDescriptor(GAPI._VulkanDevice, _DefferedDescriptors);

	/*===== CPU SIDE IMAGE BUFFERS ======*/

	//reallocate the fullscreen GBuffer and present iamges render target with the new number of available images
	VulkanHelper::AllocateFrameBuffer(_GBUfferPipelineOutput, GAPI._vk_width, GAPI._vk_height, GAPI._nb_vk_frames);
	VulkanHelper::AllocateFrameBuffer(_DefferedPipelineOutput, GAPI._vk_width, GAPI._vk_height, GAPI._nb_vk_frames);

	/*===== DESCRIPTORS ======*/

	VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _GBufferDescriptors, GAPI._nb_vk_frames);
	VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, GAPI._nb_vk_frames);

	/*===== GPU SIDE IMAGE BUFFERS ======*/
	
	_GBuffers.Alloc(3);
	CreateFrameBuffer(GAPI._VulkanUploader, _GBuffers[0], _GBUfferPipelineOutput, 0);
	CreateFrameBuffer(GAPI._VulkanUploader, _GBuffers[1], _GBUfferPipelineOutput, 1);
	CreateFrameBuffer(GAPI._VulkanUploader, _GBuffers[2], _GBUfferPipelineOutput, 2);

	/*===== CREATING & LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{

		//make our gbuffer linked to the framebuffers
		VkImageView g_buffers[3] = { _GBuffers[0]._ImageViews[i], _GBuffers[1]._ImageViews[i], _GBuffers[2]._ImageViews[i] };
		VulkanHelper::SetFrameBuffer(GAPI._VulkanUploader, _GBUfferPipelineOutput, g_buffers, i);

		//make our framebuffer linked to the swapchain back buffers
		VulkanHelper::SetFrameBuffer(GAPI._VulkanUploader, _DefferedPipelineOutput, &GAPI._VulkanBackColourBuffers[i], i);

		//wrinting our newly created imatge view in descriptor.
		//sampler is immutable so no need to send it.
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, _GBuffers[0]._ImageViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, i);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, _GBuffers[1]._ImageViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, i);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, _GBuffers[2]._ImageViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3, i);
	
	}
}



void DefferedRendering::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);

}

/*===== Act =====*/

void DefferedRendering::Act(struct AppWideContext& AppContext)
{
	//it will change every frame
	{
		_UniformBuffer._proj = perspective_proj(_GBUfferPipelineOutput._OutputScissor.extent.width, _GBUfferPipelineOutput._OutputScissor.extent.height, AppContext.fov, AppContext.near_plane, AppContext.far_plane);
		_UniformBuffer._view = transpose(AppContext.view_mat);
		_UniformBuffer._view.x.w = 0.0f;
		_UniformBuffer._view.y.w = 0.0f;
		_UniformBuffer._view.z.w = 0.0f;
		_UniformBuffer._view.w.xyz = AppContext.camera_pos;
	}


	//UI update
	if (SceneCanShowUI(AppContext))
	{
		changedFlag |= ImGui::SliderFloat3("Object Postion", _ObjData.pos.scalar, -100.0f, 100.0f);
		changedFlag |= ImGui::SliderFloat3("Object Rotation", _ObjData.euler_angles.scalar, -180.0f, 180.0f);
		changedFlag |= ImGui::SliderFloat3("Object Scale", _ObjData.scale.scalar, 0.0f, 1.0f, "%0.01f");

	}

	_ObjData.euler_angles.y += 20.0f * AppContext.delta_time;
}

/*===== Show =====*/

void DefferedRendering::Show(GAPIHandle& GAPIHandle)
{
	//to record errors
	VkResult result = VK_SUCCESS;

	//current frames resources
	VkCommandBuffer commandBuffer = GAPIHandle.GetCurrentVulkanCommand();
	VkSemaphore waitSemaphore = GAPIHandle.GetCurrentCanPresentSemaphore();
	VkSemaphore signalSemaphore = GAPIHandle.GetCurrentHasPresentedSemaphore();

	{
		//first, reset previous records
		VK_CALL_PRINT(vkResetCommandBuffer(commandBuffer, 0));

		//then open for record
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CALL_PRINT(vkBeginCommandBuffer(commandBuffer, &info));

	}

	//begin render pass onto whole screen
	{
		//Set output and output settings for this render pass.
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = _CopyPipelineOutput._OutputRenderPass;
		info.framebuffer = _CopyPipelineOutput._OuputFrameBuffer[GAPIHandle._vk_frame_index];
		info.renderArea = _CopyPipelineOutput._OutputScissor;
		info.clearValueCount = 1;

		//clear our output (grey for backbuffer)
		VkClearValue clearValue{};
		clearValue.color = { 0.2f, 0.2f, 0.2f, 1.0f };
		info.pClearValues = &clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//bind pipeline, descriptor, viewport and scissors ...
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _CopyPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _CopyLayout, 0, 1, &_CopyDescriptors._DescriptorSets[GAPIHandle._vk_current_frame], 0, nullptr);
	vkCmdSetViewport(commandBuffer, 0, 1, &_CopyPipelineOutput._OutputViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_CopyPipelineOutput._OutputScissor);

	// ... then draw !
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	//end writing in our backbuffer
	vkCmdEndRenderPass(commandBuffer);
	{
		//the pipeline stage at which the GPU should waait for the semaphore to signal itself
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR

		//we submit our commands, while setting the necesssary fences (semaphores on GPU), 
		//to schedule work properly
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &waitSemaphore;
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &commandBuffer;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &signalSemaphore;

		//closing our command record ...
		VK_CALL_PRINT(vkEndCommandBuffer(commandBuffer));
		//... and submit it straight away
		VK_CALL_PRINT(vkQueueSubmit(GAPIHandle._VulkanQueues[0], 1, &info, nullptr));
	}
}

/*===== Close =====*/

void DefferedRendering::Close(class GraphicsAPIManager& GAPI)
{
	//the memory allocated by the Vulkan Helper is volatile : it must be explecitly freed !

	//Clear geometry data
	ClearMesh(GAPI._VulkanDevice, _RayModel._Meshes[0]);
	VulkanHelper::ClearSceneBuffer(GAPI._VulkanDevice, _RaySceneBuffer);

	//clear Acceleration Structures
	VulkanHelper::ClearRaytracedGeometry(GAPI._VulkanDevice, _RayBottomAS);
	VulkanHelper::ClearRaytracedGeometry(GAPI._VulkanDevice, _RaySphereBottomAS);
	VulkanHelper::ClearRaytracedGroup(GAPI._VulkanDevice, _RayTopAS);

	//release descriptors

	ClearPipelineDescriptor(GAPI._VulkanDevice, _CopyDescriptors);
	ClearPipelineDescriptor(GAPI._VulkanDevice, _RayPipelineStaticDescriptor);
	ClearPipelineDescriptor(GAPI._VulkanDevice, _RayPipelineDynamicDescriptor);
	vkDestroySampler(GAPI._VulkanDevice, _CopySampler, nullptr);


	//clear buffers
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _RayUniformBuffer);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereBuffer);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereColour);
	ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereOffsets);
	for (uint32_t i = 0; i < 3; i++)
	{
		ClearStaticBufferHandle(GAPI._VulkanDevice, _RaySphereAABBBuffer[i]);
	}

	//clear shader resources
	ClearShaderBindingTable(GAPI._VulkanDevice, _RayShaderBindingTable);
	CLEAR_LIST(_RayShaders, _RayShaders.Nb(), VulkanHelper::ClearVulkanShader, GAPI._VulkanDevice);
	CLEAR_LIST(_CopyShaders, _CopyShaders.Nb(), VulkanHelper::ClearVulkanShader, GAPI._VulkanDevice);


	//clear the fullscreen images buffer
	VulkanHelper::ClearPipelineOutput(GAPI._VulkanDevice, _CopyPipelineOutput);
	ClearFrameBuffer(GAPI._VulkanDevice, _RayFramebuffer);

	//release pipeline objects
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _RayLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _RayPipeline, nullptr);
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _CopyLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _CopyPipeline, nullptr);
}
