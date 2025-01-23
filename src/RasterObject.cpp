#include "RasterObject.h"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"

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
	positionBinding.binding		= 0;//in the shader this comes at binding = 0
	positionBinding.inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;
	positionBinding.stride		= sizeof(vec3);

	VkVertexInputBindingDescription texCoordBinding = {};
	texCoordBinding.binding		= 1;//in the shader this comes at binding = 1
	texCoordBinding.inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;
	texCoordBinding.stride		= sizeof(vec2);

	VkVertexInputBindingDescription normalBinding = {};
	normalBinding.binding	= 2;//in the shader this comes at binding = 2
	normalBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	normalBinding.stride	= sizeof(vec3);

	VkVertexInputBindingDescription inputBinding[3] = { positionBinding , texCoordBinding, normalBinding };

	//positions ; vec3
	VkVertexInputAttributeDescription positionAttribute = {};
	positionAttribute.binding	= 0;
	positionAttribute.format	= VK_FORMAT_R32G32B32_SFLOAT;
	positionAttribute.location	= 0;
	positionAttribute.offset	= 0;
	
	//uvs ; vec2
	VkVertexInputAttributeDescription texCoordAttribute = {};
	texCoordAttribute.binding	= 1;
	texCoordAttribute.format	= VK_FORMAT_R32G32_SFLOAT;
	texCoordAttribute.location	= 1;
	texCoordAttribute.offset	= 0;

	//normals ; vec3
	VkVertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding		= 2;
	normalAttribute.format		= VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.location	= 2;
	normalAttribute.offset		= 0;

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
	viewportStateInfo.pViewports	= &_ObjViewport;
	viewportStateInfo.scissorCount	= 1;
	viewportStateInfo.pScissors		= &_ObjScissors;

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

	//description of colour bkending and writing configuration : here we write every component of the output, but don't do alpha blending or other stuff
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
		frameColourBufferAttachment.format			= GAPI._VulkanSurfaceFormat.format;
		frameColourBufferAttachment.samples			= VK_SAMPLE_COUNT_1_BIT;//one pixel will have exactly one calculation done
		frameColourBufferAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;//we'll make the pipeline clear
		frameColourBufferAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;//we want to write into the framebuffer
		frameColourBufferAttachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;//we write onto the entire vewport so it will be completely replaced, what was before does not interests us
		frameColourBufferAttachment.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//there is no post process this is immediate writing into frame buffer

		/*===== DEPTH BUFFER CONFIG =====*/

		//describing the format of the depthbuffer
		VkAttachmentDescription depthBufferAttachment{};
		depthBufferAttachment.format			= VK_FORMAT_D32_SFLOAT;
		depthBufferAttachment.samples			= VK_SAMPLE_COUNT_1_BIT;//one pixel will have exactly one calculation done
		depthBufferAttachment.loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;//we'll make the pipeline clear the depth buffer
		depthBufferAttachment.storeOp			= VK_ATTACHMENT_STORE_OP_DONT_CARE;//we just want the rasterizer to use it
		depthBufferAttachment.stencilLoadOp		= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthBufferAttachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthBufferAttachment.initialLayout		= VK_IMAGE_LAYOUT_UNDEFINED;//we write onto the entire vewport so it will be completely replaced, what was before does not interests us
		depthBufferAttachment.finalLayout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;//we just want to use it as depth buffer.

		/*===== SUBPASS DESCRIPTION ======*/

		//describing the output in the subpass
		VkAttachmentReference frameColourBufferAttachmentRef{};
		frameColourBufferAttachmentRef.attachment	= 0;
		frameColourBufferAttachmentRef.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//describing the output in the subpass
		VkAttachmentReference depthBufferAttachmentRef{};
		depthBufferAttachmentRef.attachment = 1;
		depthBufferAttachmentRef.layout		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//describing the subpass of our main renderpass
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount	= 1;
		subpass.pColorAttachments		= &frameColourBufferAttachmentRef;
		subpass.pDepthStencilAttachment = &depthBufferAttachmentRef;

		//describing our renderpass
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount	= 2;

		//the ouputs of our render pass are the depth buffer (even if it unused afterwards) and the back buffers
		VkAttachmentDescription attachements[2] = { frameColourBufferAttachment, depthBufferAttachment };
		renderPassInfo.pAttachments = attachements;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		//we need to be able to sample a texture
		VkSubpassDependency dependency{};
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VK_CALL_PRINT(vkCreateRenderPass(GAPI._VulkanDevice, &renderPassInfo, nullptr, &_ObjRenderPass));
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		//creating our uniform buffer
		VkDescriptorSetLayoutBinding uniformLayoutBinding{};
		uniformLayoutBinding.binding			= 0;
		uniformLayoutBinding.descriptorType		= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformLayoutBinding.descriptorCount	= 1;
		uniformLayoutBinding.stageFlags			= VK_SHADER_STAGE_VERTEX_BIT;

		//we need a descriptor for our uniform buffer
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings	= &uniformLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_ObjBufferDescriptorLayout));

		//and we need a descriptor to use a sampler (said sampler will be created after, when loading the model)
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding			= 1;
		samplerLayoutBinding.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount	= 1;
		samplerLayoutBinding.stageFlags			= VK_SHADER_STAGE_FRAGMENT_BIT;

		layoutInfo.pBindings = &samplerLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_ObjSamplerDescriptorLayout));

	}

	{
		//our pipeline will, in total, only use two descriptors ...
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		
		//one to get the 3D matrices, and one to sample from a texture
		VkDescriptorSetLayout layouts[2] = {_ObjBufferDescriptorLayout, _ObjSamplerDescriptorLayout};
		pipelineLayoutInfo.pSetLayouts = layouts;
		pipelineLayoutInfo.setLayoutCount = 2;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_ObjLayout));
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
	pipelineInfo.layout					= _ObjLayout;
	pipelineInfo.renderPass				= _ObjRenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_ObjPipeline));

	vkDestroyShaderModule(GAPI._VulkanDevice, FragmentShader, nullptr);
	vkDestroyShaderModule(GAPI._VulkanDevice, VertexShader, nullptr);


	/*===== MODEL LOADING =====*/

	//load vertex buffer and textures
	VulkanHelper::LoadGLTFFile(GAPI._VulkanUploader, "../../../media/Duck/Duck.gltf",_ObjModel);

	/*===== MODEL DESCRIPTORS ======*/

	{
		//describing how many descriptor at a time should be allocated
		VkDescriptorPoolSize poolSamplerSize{};
		poolSamplerSize.type			= VK_DESCRIPTOR_TYPE_SAMPLER;
		poolSamplerSize.descriptorCount = 3;//we only support PBR with albedo, metal-rough and normal texture


		//creating our descriptor pool to allocate sets for each textures, as we chose to have combined samplers
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 1;
		poolInfo.pPoolSizes		= &poolSamplerSize;
		poolInfo.maxSets		= _ObjModel._Materials.Nb();

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_ObjSamplerDescriptorPool));


		//describe a descriptor set for each of our combined sampler (we only allow three textures per material)
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= _ObjSamplerDescriptorPool;
		allocInfo.descriptorSetCount	= _ObjModel._Materials.Nb();

		//copying the same layout for every descriptor set
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(_ObjModel._Materials.Nb() * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < _ObjModel._Materials.Nb(); i++)
			memcpy(&layouts[i], &_ObjSamplerDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//then create the resource
		_ObjSamplerDescriptorSet.Alloc(_ObjModel._Materials.Nb());
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_ObjSamplerDescriptorSet));

	}

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < _ObjModel._Materials.Nb(); i++)
	{
		//the order of the textures in the material need to be ALBEDO, METAL-ROUGH, then NORMAL to actually work...
		for (uint32_t j = 0; j < _ObjModel._Materials[i]._Textures.Nb(); j++)
		{
			//describing our combined sampler
			VkDescriptorImageInfo samplerInfo{};
			samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			samplerInfo.imageView	= _ObjModel._Materials[i]._Textures[j]._ImageView;
			samplerInfo.sampler		= _ObjModel._Materials[i]._Textures[j]._Sampler;

			//then link it to the descriptor (one texture at a time)
			VkWriteDescriptorSet samplerWrite{};
			samplerWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			samplerWrite.dstSet				= _ObjSamplerDescriptorSet[i];
			samplerWrite.dstBinding			= 1;
			samplerWrite.dstArrayElement	= j;
			samplerWrite.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerWrite.descriptorCount	= 1;
			samplerWrite.pImageInfo			= &samplerInfo;

			vkUpdateDescriptorSets(GAPI._VulkanDevice, 1, &samplerWrite, 0, nullptr);
		}
		_ObjModel._Materials[i]._TextureDescriptors = _ObjSamplerDescriptorSet[i];
		
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


	CompileVulkanShaders(GAPI._VulkanUploader, VertexShader, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raster Object Vertex");
	CompileVulkanShaders(GAPI._VulkanUploader, FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raster Object Frag");
}

void RasterObject::Prepare(class GraphicsAPIManager& GAPI)
{
	//the shaders needed
	VkShaderModule VertexShader, FragmentShader;

	//compile the shaders here...
	PrepareVulkanScripts(GAPI, VertexShader, FragmentShader);
	//then create the pipeline
	PrepareVulkanProps(GAPI, VertexShader, FragmentShader);

	//a "zero init" of the transform values
	_ObjData.scale = vec3{ 1.0f, 1.0f, 1.0f };
	_ObjData.pos = vec3{ 0.0f, 0.0f, 1.0f };

	enabled = true;
}

/*===== Resize =====*/

void RasterObject::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//first, we clear previously used resources
	VK_CLEAR_ARRAY(_ObjOutput, (uint32_t)old_nb_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _ObjBufferDescriptorPool, nullptr);

	/*===== VIEWPORT AND SCISSORS ======*/

	//filling up the viewport...
	_ObjViewport.width = (float)GAPI._vk_width;
	_ObjViewport.height = (float)GAPI._vk_height;
	_ObjViewport.maxDepth = 1.0f;
	_ObjViewport.minDepth = 0.0f;
	_ObjViewport.x = 0.0f;
	_ObjViewport.y = 0.0f;

	//... and the scissors
	_ObjScissors.extent = { (uint32_t)GAPI._vk_width, (uint32_t)GAPI._vk_height };

	/*===== DESCRIPTORS ======*/

	{
		//describing how many descriptor at a time should be allocated
		VkDescriptorPoolSize poolUniformSize{};
		poolUniformSize.type			= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolUniformSize.descriptorCount = 1;

		//creating our descriptor pool to allocate sets for each frame
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 1;
		poolInfo.pPoolSizes		= &poolUniformSize;
		poolInfo.maxSets		= GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_ObjBufferDescriptorPool))

		//allocating a descriptor set for each of our framebuffer (a uniform buffer per frame)
		VkDescriptorSetAllocateInfo allocInfo {};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= _ObjBufferDescriptorPool;
		allocInfo.descriptorSetCount	= GAPI._nb_vk_frames;

		//copying the same layout for every descriptor set
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_ObjBufferDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//then create the resource
		_ObjBufferDescriptorSet.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_ObjBufferDescriptorSet));
	}

	/*===== FRAMEBUFFERS ======*/

	//creating the description of the output of our pipeline
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = _ObjRenderPass;
	framebufferInfo.width = GAPI._vk_width;
	framebufferInfo.height = GAPI._vk_height;
	framebufferInfo.layers = 1;
	//allocating space for our outputs
	_ObjOutput.Alloc(GAPI._nb_vk_frames);

	/*===== UNIFORM BUFFERS ======*/

	//recreate the uniform buffer
	CreateUniformBufferHandle(GAPI._VulkanUploader, _ObjMatBufferHandle, GAPI._nb_vk_frames, sizeof(mat4) * 3);

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//link our framebuffers (our pipeline's output) to the backbuffers
		framebufferInfo.attachmentCount = 2;
		VkImageView attachement[2] = { GAPI._VulkanBackColourBuffers[i] , GAPI._VulkanDepthBufferViews[i] };
		framebufferInfo.pAttachments = attachement;
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI._VulkanDevice, &framebufferInfo, nullptr, &_ObjOutput[i]))

		//describing our uniform matrices buffer
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer	= _ObjMatBufferHandle._GPUBuffer[i];
		bufferInfo.offset	= 0;
		bufferInfo.range	= sizeof(mat4) * 3;

		//then link it to the descriptor
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet			= _ObjBufferDescriptorSet[i];
		descriptorWrite.dstBinding		= 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(GAPI._VulkanDevice, 1, &descriptorWrite, 0, nullptr);
	}
}


void RasterObject::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
}

/*===== Act =====*/

void RasterObject::Act(struct AppWideContext& AppContext)
{

	_ObjData.euler_angles.y += AppContext.delta_time;

	//it will change every frame
	{
		_ObjBuffer.proj = AppContext.proj_mat;
		_ObjBuffer.view = AppContext.view_mat;
		_ObjBuffer.model = scale(_ObjData.scale.x, _ObjData.scale.y, _ObjData.scale.z) * intrinsic_rot(_ObjData.euler_angles.x, _ObjData.euler_angles.y, _ObjData.euler_angles.z) * translate(_ObjData.pos);
	}

	//UI update
	if (SceneCanShowUI(AppContext))
	{
		ImGui::SliderFloat3("Object Postion", _ObjData.pos.scalar, -100.0f, 100.0f);
		ImGui::SliderFloat3("Object Rotation", _ObjData.euler_angles.scalar, -180.0f, 180.0f);
		ImGui::SliderFloat3("Object Scale", _ObjData.scale.scalar, 0.0f, 1.0f, "%0.01f");
	}

}

/*===== Show =====*/

void RasterObject::Show(GAPIHandle& GAPIHandle)
{
	//to record errors
	VkResult result = VK_SUCCESS;

	//current frames resources
	VkCommandBuffer commandBuffer	= GAPIHandle.GetCurrentVulkanCommand();
	VkSemaphore waitSemaphore		= GAPIHandle.GetCurrentCanPresentSemaphore();
	VkSemaphore signalSemaphore		= GAPIHandle.GetCurrentHasPresentedSemaphore();

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
		info.renderPass					= _ObjRenderPass;//obviously using this scene's renderpass
		info.framebuffer				= _ObjOutput[GAPIHandle._vk_frame_index];//writing in the available backbuffer
		info.renderArea.extent.width	= (uint32_t)_ObjViewport.width;//writing on the whole screen
		info.renderArea.extent.height	= (uint32_t)_ObjViewport.height;//writing on the whole screen
		info.clearValueCount = 2;

		//clear our outputs (grey for backbuffer, infinity for depth buffer)
		VkClearValue clearValue [2] = {};
		clearValue[0].color			= {0.2f, 0.2f, 0.2f, 1.0f};
		clearValue[1].depthStencil	= {1.0f, 0};
		info.pClearValues			= clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//the buffer will change every frame as the object rotates every frame
	memcpy(_ObjMatBufferHandle._CPUMemoryHandle[GAPIHandle._vk_current_frame], (void*)&_ObjBuffer, sizeof(mat4) * 3);

	//binding the pipeline to asteriwe our model
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _ObjPipeline);
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _ObjLayout, 0, 1, &_ObjBufferDescriptorSet[GAPIHandle._vk_current_frame], 0, nullptr);

	//set viewport and scissors to draw on all screen
	vkCmdSetViewport(commandBuffer, 0, 1, &_ObjViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_ObjScissors);

	//draw all meshes, as model may be composed of multiple meshes
	for (uint32_t i = 0; i < _ObjModel._Meshes.Nb(); i++)
	{
		//first bind the "material", basically the three compibined sampler descriptors ...
		if (_ObjModel._Materials.Nb() > 0)
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _ObjLayout, 1, 1, &_ObjModel._Materials[_ObjModel._material_index[i]]._TextureDescriptors, 0, nullptr);
		//... then bind the vertex buffer as described in the input layout of the pipeline ...
		vkCmdBindVertexBuffers(commandBuffer, 0, 3, _ObjModel._Meshes[i]._VertexBuffers, (VkDeviceSize*)_ObjModel._Meshes[i]._vertex_offsets);
		//... and the index buffers associated with the vertex buffers ...
		vkCmdBindIndexBuffer(commandBuffer, _ObjModel._Meshes[i]._Indices, 0, _ObjModel._Meshes[i]._indices_type);
		//... before finally drawing, following the index buffer.
		vkCmdDrawIndexed(commandBuffer, _ObjModel._Meshes[i]._indices_nb, 1, _ObjModel._Meshes[i]._indices_offset, 0, 0);
	}


	//end writing in our backbuffer
	vkCmdEndRenderPass(commandBuffer);
	{
		//the pipeline stage at which the GPU should waait for the semaphore to signal itself
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		//we submit our commands, while setting the necesssary fences (semaphores on GPU), 
		//to schedule work properly
		VkSubmitInfo info = {};
		info.sType					= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount		= 1;
		info.pWaitSemaphores		= &waitSemaphore;
		info.pWaitDstStageMask		= &wait_stage;
		info.commandBufferCount		= 1;
		info.pCommandBuffers		= &commandBuffer;
		info.signalSemaphoreCount	= 1;
		info.pSignalSemaphores		= &signalSemaphore;

		//closing our command record ...
		VK_CALL_PRINT(vkEndCommandBuffer(commandBuffer));
		//... and submit it straight away
		VK_CALL_PRINT(vkQueueSubmit(GAPIHandle._VulkanQueues[0], 1, &info, nullptr));
	}
}

/*===== Close =====*/

void RasterObject::Close(class GraphicsAPIManager& GAPI)
{
	//releasing the "per frames" objects
	VK_CLEAR_ARRAY(_ObjOutput, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
	ClearUniformBufferHandle(GAPI._VulkanDevice, _ObjMatBufferHandle);

	//the memory allocated by the Vulkan Helper is volatile : it must be explecitly freed !
	ClearModel(GAPI._VulkanDevice, _ObjModel);

	//release descriptors
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _ObjBufferDescriptorPool, nullptr);
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _ObjSamplerDescriptorPool, nullptr);
	_ObjBufferDescriptorSet.Clear();
	_ObjSamplerDescriptorSet.Clear();
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _ObjBufferDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _ObjSamplerDescriptorLayout, nullptr);

	//release pipeline objects
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _ObjLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _ObjPipeline, nullptr);
	vkDestroyRenderPass(GAPI._VulkanDevice, _ObjRenderPass, nullptr);
}
