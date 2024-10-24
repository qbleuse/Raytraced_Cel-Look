#include "RasterObject.h"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"

//include loader app
#include "tiny_obj_loader.h"

/*==== Prepare =====*/

void RasterObject::PrepareVulkanProps(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{
	VkResult result = VK_SUCCESS;

	/*===== SHADER ======*/

	//describe vertex shader stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage	= VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module	= VertexShader;
	vertShaderStageInfo.pName	= "main";

	//describe fragment shader stage
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType	= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage	= VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module	= FragmentShader;
	fragShaderStageInfo.pName	= "main";

	/*===== VERTEX INPUT ======*/

	VkVertexInputBindingDescription positionBinding = {};
	positionBinding.binding	= 0;//in the shader this comes at binding = 0
	positionBinding.inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;
	positionBinding.stride		= sizeof(vec3);

	VkVertexInputBindingDescription texCoordBinding = {};
	texCoordBinding.binding = 1;//in the shader this comes at binding = 1
	texCoordBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	texCoordBinding.stride = sizeof(vec2);

	VkVertexInputBindingDescription normalBinding = {};
	normalBinding.binding	= 2;//in the shader this comes at binding = 2
	normalBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	normalBinding.stride	= sizeof(vec3);

	VkVertexInputBindingDescription inputBinding[3] = { positionBinding , texCoordBinding, normalBinding };

	VkVertexInputAttributeDescription positionAttribute = {};
	positionAttribute.binding	= 0;
	positionAttribute.format	= VK_FORMAT_R32G32B32_SFLOAT;
	positionAttribute.location	= 0;
	positionAttribute.offset	= 0;

	VkVertexInputAttributeDescription texCoordAttribute = {};
	texCoordAttribute.binding = 1;
	texCoordAttribute.format	= VK_FORMAT_R32G32_SFLOAT;
	texCoordAttribute.location = 1;
	texCoordAttribute.offset = 0;

	VkVertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding = 2;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.location = 2;
	normalAttribute.offset	= 0;

	VkVertexInputAttributeDescription inputAttribute[3] = {positionAttribute, texCoordAttribute, normalAttribute};

	//the vertex input for the input assembly stage. in this scene, there will not be any vertices, so we'll fill it up with nothing.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType							= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount	= 3;
	vertexInputInfo.pVertexBindingDescriptions		= inputBinding; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 3;
	vertexInputInfo.pVertexAttributeDescriptions	= inputAttribute; // Optional

	//the description of the input assembly stage. by the fact we won't have any vertex, the input assembly will basically only give the indices (basically 0,1,2 if we ask for a draw command of three vertices)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType		= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	/*===== VIEWPORT/SCISSORS  =====*/

	//most things are described in advance in vulkan pipeline, but this is the config of thing that can be changed on the fly. in our situation it will be only Viewport.
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount	= 2;
	VkDynamicState viewportState[2]		= { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	dynamicStateInfo.pDynamicStates		= viewportState;

	//describing our viewport (even it is still dynamic)
	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports	= &objectViewport;
	viewportStateInfo.scissorCount	= 1;
	viewportStateInfo.pScissors		= &objectScissors;


	/*==== RASTERIZER =====*/

	//the description of our rasterizer : a very simple one, that will just fill up polygon (in this case one)
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.polygonMode		= VK_POLYGON_MODE_FILL;
	rasterizer.cullMode			= VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace		= VK_FRONT_FACE_CLOCKWISE;
	rasterizer.lineWidth		= 1.0f;

	/*===== MSAA =====*/

	//the description of the MSAA, here we just don't want it, an aliased triangle is more than enough
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable	= VK_FALSE;
	multisampling.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	/*===== FRAMEBUFFER BLEND ======*/

	//description of colour bkending and writing configuration : here we write every component of the output, but don;t do alpha blending or other stuff
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable	= VK_FALSE;

	//description of colour blending :  here it is a simple write and discard old.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable		= VK_FALSE;
	colorBlending.attachmentCount	= 1;
	colorBlending.pAttachments		= &colorBlendAttachment;

	/*===== DEPTH TEST ======*/
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType				= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable	= VK_TRUE;
	depthStencil.depthWriteEnable	= VK_TRUE;
	depthStencil.depthCompareOp		= VK_COMPARE_OP_LESS;

	{
		/*===== FRAMEBUFFER OUTPUT ======*/

		//describing the format of the output (our framebuffers)
		VkAttachmentDescription frameColourBufferAttachment{};
		frameColourBufferAttachment.format = GAPI.VulkanSurfaceFormat.format;
		frameColourBufferAttachment.samples = VK_SAMPLE_COUNT_1_BIT;//one pixel will have exactly one calculation done
		frameColourBufferAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//we'll make the pipeline clear
		frameColourBufferAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//we want to write into the framebuffer
		frameColourBufferAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//we write onto the entire vewport so it will be completely replaced, what was before does not interests us
		frameColourBufferAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//there is no post process this is immediate writing into frame buffer

		/*===== DEPTH BUFFER CONFIG =====*/

		//describing the format of the depthbuffer
		VkAttachmentDescription depthBufferAttachment{};
		depthBufferAttachment.format = VK_FORMAT_D32_SFLOAT;
		depthBufferAttachment.samples = VK_SAMPLE_COUNT_1_BIT;//one pixel will have exactly one calculation done
		depthBufferAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//we'll make the pipeline clear the depth buffer
		depthBufferAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;//we just want the rasterizer to use it
		depthBufferAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthBufferAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthBufferAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//we write onto the entire vewport so it will be completely replaced, what was before does not interests us
		depthBufferAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;//we just want to use it as depth buffer.

		/*===== SUBPASS DESCRIPTION ======*/

		//describing the output in the subpass
		VkAttachmentReference frameColourBufferAttachmentRef{};
		frameColourBufferAttachmentRef.attachment = 0;
		frameColourBufferAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//describing the output in the subpass
		VkAttachmentReference depthBufferAttachmentRef{};
		depthBufferAttachmentRef.attachment = 1;
		depthBufferAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//describing the subpass of our main renderpass
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &frameColourBufferAttachmentRef;
		subpass.pDepthStencilAttachment = &depthBufferAttachmentRef;

		//describing our renderpass
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 2;
		VkAttachmentDescription attachements[2] = { frameColourBufferAttachment, depthBufferAttachment };
		renderPassInfo.pAttachments = attachements;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		VkSubpassDependency dependency{};
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VK_CALL_PRINT(vkCreateRenderPass(GAPI.VulkanDevice, &renderPassInfo, nullptr, &objectRenderPass));
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		//creating our uniform buffer
		VkDescriptorSetLayoutBinding vertexUniformLayoutBinding{};
		vertexUniformLayoutBinding.binding = 0;
		vertexUniformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		vertexUniformLayoutBinding.descriptorCount = 1;
		vertexUniformLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &vertexUniformLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI.VulkanDevice, &layoutInfo, nullptr, &objectDescriptorLayout));

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		layoutInfo.pBindings = &samplerLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI.VulkanDevice, &layoutInfo, nullptr, &samplerDescriptorLayout));

	}

	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		VkDescriptorSetLayout layouts[2] = {objectDescriptorLayout, samplerDescriptorLayout};
		pipelineLayoutInfo.pSetLayouts = layouts;
		pipelineLayoutInfo.setLayoutCount = 2;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI.VulkanDevice, &pipelineLayoutInfo, nullptr, &objectLayout));
	}

	/*===== PIPELINE ======*/

	//finally creating our pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//putting our two shader stages
	VkPipelineShaderStageCreateInfo stages[2] = { vertShaderStageInfo, fragShaderStageInfo };
	pipelineInfo.pStages = stages;
	pipelineInfo.stageCount = 2;

	//fill up the pipeline description
	pipelineInfo.pVertexInputState		= &vertexInputInfo;
	pipelineInfo.pInputAssemblyState	= &inputAssembly;
	pipelineInfo.pViewportState			= &viewportStateInfo;
	pipelineInfo.pRasterizationState	= &rasterizer;
	pipelineInfo.pMultisampleState		= &multisampling;
	pipelineInfo.pDepthStencilState		= &depthStencil;
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= &dynamicStateInfo;
	pipelineInfo.layout					= objectLayout;
	pipelineInfo.renderPass				= objectRenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI.VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &objectPipeline));

	vkDestroyShaderModule(GAPI.VulkanDevice, FragmentShader, nullptr);
	vkDestroyShaderModule(GAPI.VulkanDevice, VertexShader, nullptr);


	/*===== VERTEX BUFFER =====*/

	VulkanHelper::LoadGLTFFile(GAPI.VulkanUploader, "../../../media/Duck/Duck.gltf",meshBuffer);

	//create descriptor for samplers
	VkDescriptorPoolSize poolSamplerSize{};
	poolSamplerSize.type			= VK_DESCRIPTOR_TYPE_SAMPLER;
	poolSamplerSize.descriptorCount = 3;//we only support PBR with albedo, PBR and normal texture

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount	= 1;
	poolInfo.pPoolSizes		= &poolSamplerSize;
	poolInfo.maxSets		= meshBuffer.materials.Nb();

	VK_CALL_PRINT(vkCreateDescriptorPool(GAPI.VulkanDevice, &poolInfo, nullptr, &meshDescriptorPool));

	//describe a descriptor set for each of our framebuffer (a uniform bufer per frame)
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool		= meshDescriptorPool;
	allocInfo.descriptorSetCount	= meshBuffer.materials.Nb();

	VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(meshBuffer.materials.Nb() * sizeof(VkDescriptorSetLayout));
	for (uint32_t i = 0; i < meshBuffer.materials.Nb(); i++)
		memcpy(&layouts[i], &samplerDescriptorLayout, sizeof(VkDescriptorSetLayout));
	allocInfo.pSetLayouts = layouts;

	meshDescriptorSet.Alloc(meshBuffer.materials.Nb());
	VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI.VulkanDevice, &allocInfo, *meshDescriptorSet));

	for (uint32_t i = 0; i < meshBuffer.materials.Nb(); i++)
	{
		for (uint32_t j = 0; j < meshBuffer.materials[i].textures.Nb(); j++)
		{
			VkDescriptorImageInfo samplerInfo{};
			samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			samplerInfo.imageView	= meshBuffer.materials[i].textures[j].imageView;
			samplerInfo.sampler		= meshBuffer.materials[i].textures[j].sampler;

			VkWriteDescriptorSet samplerWrite{};
			samplerWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			samplerWrite.dstSet				= meshDescriptorSet[i];
			samplerWrite.dstBinding			= 1;
			samplerWrite.dstArrayElement	= j;
			samplerWrite.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerWrite.descriptorCount	= 1;
			samplerWrite.pImageInfo			= &samplerInfo;

			vkUpdateDescriptorSets(GAPI.VulkanDevice, 1, &samplerWrite, 0, nullptr);
		}
		meshBuffer.materials[i].textureDescriptors = meshDescriptorSet[i];
		
	}
	
}

void RasterObject::PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{
	//define vertex shader
	const char* vertex_shader =
		R"(#version 450

			layout(location = 0) in vec3 positions;
			layout(location = 1) in vec2 inTexCoord;
			layout(location = 2) in vec3 inNormal;


			layout(binding = 0) uniform Transform
			{
				mat4 model;
				mat4 view;
				mat4 proj;
			};

			layout(location = 0) out vec3 vertPos;
			layout(location = 1) out vec2 outTexCoord;
			layout(location = 2) out vec3 outNormal;

			void main()
			{
				gl_Position =  proj * view * model * vec4(positions, 1.0);
				vertPos = (view * model * vec4(positions, 1.0)).xyz;
				outTexCoord = inTexCoord;
				outNormal = ( model * vec4(inNormal,0.0)).xyz;

			})";

	const char* fragment_shader =
		R"(#version 450
		layout(location = 0) in vec3 vertPos;
		layout(location = 1) in vec2 outTexCoord;
		layout(location = 2) in vec3 outNormal;

		layout(location = 0) out vec4 outColor;

		layout(set = 1, binding = 1) uniform sampler2D albedo;
		//layout(binding = 1, set = 1) uniform sampler2D pbr;
		//layout(binding = 1, set = 2) uniform sampler2D normal;


		void main()
		{
			vec3 lightDir = normalize(vec3(1.0,1.0,1.0));
			vec3 viewDir = normalize(-vertPos);

			vec3 halfDir = normalize(viewDir+lightDir);
			float specAngle = max(dot(halfDir,outNormal), 0.0);
			float specular = pow(specAngle,16.0);
			float diffuse = max(dot(lightDir, outNormal),0.0);

			outColor = vec4((diffuse + specular) * texture(albedo,outTexCoord).rgb,1.0);
		}
		)";


	CreateVulkanShaders(GAPI.VulkanUploader, VertexShader, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raster Object Vertex");
	CreateVulkanShaders(GAPI.VulkanUploader, FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raster Object Frag");
}

void RasterObject::Prepare(class GraphicsAPIManager& GAPI)
{
	VkShaderModule VertexShader, FragmentShader;

	PrepareVulkanScripts(GAPI, VertexShader, FragmentShader);
	PrepareVulkanProps(GAPI, VertexShader, FragmentShader);

	oData.scale = vec3{ 1.0f, 1.0f, 1.0f };
	oData.pos = vec3{ 0.0f, 0.0f, 1.0f };
}

/*===== Resize =====*/

void RasterObject::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	VK_CLEAR_ARRAY(objectOutput, (uint32_t)old_nb_frames, vkDestroyFramebuffer, GAPI.VulkanDevice);
	vkDestroyDescriptorPool(GAPI.VulkanDevice, objectDescriptorPool, nullptr);


	//creating the description of the output of our pipeline
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = objectRenderPass;
	framebufferInfo.width = GAPI.VulkanWidth;
	framebufferInfo.height = GAPI.VulkanHeight;
	framebufferInfo.layers = 1;

	//filling up the viewport
	objectViewport.width = (float)GAPI.VulkanWidth;
	objectViewport.height = (float)GAPI.VulkanHeight;
	objectViewport.maxDepth = 1.0f;
	objectViewport.minDepth = 0.0f;
	objectViewport.x = 0.0f;
	objectViewport.y = 0.0f;

	objectScissors.extent = { (uint32_t)GAPI.VulkanWidth, (uint32_t)GAPI.VulkanHeight };

	objectOutput.Alloc(GAPI.NbVulkanFrames);

	//create the descriptor pool
	VkDescriptorPoolSize poolUniformSize{};
	poolUniformSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolUniformSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolUniformSize;
	poolInfo.maxSets = GAPI.NbVulkanFrames;

	VK_CALL_PRINT(vkCreateDescriptorPool(GAPI.VulkanDevice, &poolInfo, nullptr, &objectDescriptorPool))

	//describe a descriptor set for each of our framebuffer (a uniform bufer per frame)
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = objectDescriptorPool;
	allocInfo.descriptorSetCount = GAPI.NbVulkanFrames;
	VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI.NbVulkanFrames * sizeof(VkDescriptorSetLayout));
	for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
		memcpy(&layouts[i], &objectDescriptorLayout, sizeof(VkDescriptorSetLayout));
	allocInfo.pSetLayouts = layouts;

	objectDescriptorSet.Alloc(GAPI.NbVulkanFrames);
	VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI.VulkanDevice, &allocInfo, *objectDescriptorSet));

	//recreate the uniform bufferx
	CreateUniformBufferHandle(GAPI.VulkanUploader, matBufferHandle, GAPI.NbVulkanFrames, sizeof(mat4) * 3);

	for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
	{
		framebufferInfo.attachmentCount = 2;
		VkImageView attachement[2] = { GAPI.VulkanBackColourBuffers[i] , GAPI.VulkanDepthBufferViews[i] };
		framebufferInfo.pAttachments = attachement;
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI.VulkanDevice, &framebufferInfo, nullptr, &objectOutput[i]))


		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = matBufferHandle.GPUBuffer[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(mat4) * 3;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet			= objectDescriptorSet[i];
		descriptorWrite.dstBinding		= 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(GAPI.VulkanDevice, 1, &descriptorWrite, 0, nullptr);
	}
}


void RasterObject::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
}

/*===== Act =====*/

void RasterObject::Act(struct AppWideContext& AppContext)
{

	oData.euler_angles.y += AppContext.delta_time;

	//it will change every frame
	{
		matBuffer.proj = AppContext.proj_mat;
		matBuffer.view = AppContext.view_mat;
		matBuffer.model = scale(oData.scale.x, oData.scale.y, oData.scale.z) * ro_intrinsic_rot(oData.euler_angles.x, oData.euler_angles.y, oData.euler_angles.z) * ro_translate(oData.pos);
	}

	//UI update
	if (SceneCanShowUI(AppContext))
	{
		ImGui::SliderFloat3("Object Postion", oData.pos.scalar, -100.0f, 100.0f);
		ImGui::SliderFloat3("Object Rotation", oData.euler_angles.scalar, -180.0f, 180.0f);
		ImGui::SliderFloat3("Object Scale", oData.scale.scalar, 0.0f, 100.0f, "%0.01f");
	}

}

/*===== Show =====*/

void RasterObject::Show(GAPIHandle& GAPIHandle)
{
	VkResult err;
	VkCommandBuffer commandBuffer = GAPIHandle.GetCurrentVulkanCommand();
	VkSemaphore waitSemaphore = GAPIHandle.GetCurrentCanPresentSemaphore();
	VkSemaphore signalSemaphore = GAPIHandle.GetCurrentHasPresentedSemaphore();

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
		info.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass					= objectRenderPass;
		info.framebuffer				= objectOutput[GAPIHandle.VulkanFrameIndex];
		info.renderArea.extent.width	= (uint32_t)objectViewport.width;
		info.renderArea.extent.height	= (uint32_t)objectViewport.height;
		info.clearValueCount = 2;
		VkClearValue clearValue [2] = {};
		clearValue[0].color = {0.2f, 0.2f, 0.2f, 1.0f};
		clearValue[1].depthStencil = {1.0f, 0};
		info.pClearValues = clearValue;
		//info.pClearValues				= &imgui.ImGuiWindow.ClearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//the buffer will change every frame as the object rotates every frame
	memcpy(matBufferHandle.CPUMemoryHandle[GAPIHandle.VulkanCurrentFrame], (void*)&matBuffer, sizeof(mat4) * 3);


	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objectPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objectLayout, 0, 1, &objectDescriptorSet[GAPIHandle.VulkanCurrentFrame], 0, nullptr);


	vkCmdSetViewport(commandBuffer, 0, 1, &objectViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &objectScissors);

	//draw all meshes
	for (uint32_t i = 0; i < meshBuffer.meshes.Nb(); i++)
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objectLayout, 1, 1, &meshBuffer.materials[meshBuffer.materialIndex[i]].textureDescriptors, 0, nullptr);
		vkCmdBindVertexBuffers(commandBuffer, 0, 3, meshBuffer.meshes[i].vertexBuffers, (VkDeviceSize*)meshBuffer.meshes[i].vertexOffsets);
		vkCmdBindIndexBuffer(commandBuffer, meshBuffer.meshes[i].indices, 0, meshBuffer.meshes[i].indicesType);
		vkCmdDrawIndexed(commandBuffer, meshBuffer.meshes[i].indicesNb, 1, meshBuffer.meshes[i].indicesOffset, 0, 0);
	}


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
		err = vkQueueSubmit(GAPIHandle.VulkanQueues[0], 1, &info, nullptr);
		//check_vk_result(err);
	}
}

/*===== Close =====*/

void RasterObject::Close(class GraphicsAPIManager& GAPI)
{
	VK_CLEAR_ARRAY(objectOutput, GAPI.NbVulkanFrames, vkDestroyFramebuffer, GAPI.VulkanDevice);

	ClearUniformBufferHandle(GAPI.VulkanDevice, matBufferHandle);
	//for (uint32_t i = 0; i < meshBuffer.Nb(); i++)
	ClearModel(GAPI.VulkanDevice, meshBuffer);//they all use the same buffers

	vkDestroyDescriptorPool(GAPI.VulkanDevice, objectDescriptorPool, nullptr);
	vkDestroyDescriptorPool(GAPI.VulkanDevice, meshDescriptorPool, nullptr);


	vkDestroyDescriptorSetLayout(GAPI.VulkanDevice, objectDescriptorLayout, nullptr);
	vkDestroyPipelineLayout(GAPI.VulkanDevice, objectLayout, nullptr);
	vkDestroyPipeline(GAPI.VulkanDevice, objectPipeline, nullptr);
	vkDestroyRenderPass(GAPI.VulkanDevice, objectRenderPass, nullptr);
}
