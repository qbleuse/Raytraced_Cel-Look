#include "DefferedRendering.h"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"
#include "ImGuiHelper.h"

/*==== Prepare =====*/

void DefferedRendering::PrepareVulkanProps(GraphicsAPIManager& GAPI)
{
	PrepareModelProps(GAPI);
	PrepareGBufferProps(GAPI);
	PrepareDefferedPassProps(GAPI);
}

//Creates the model and the other necessary resources 
void DefferedRendering::PrepareModelProps(class GraphicsAPIManager& GAPI)
{
	VkResult result = VK_SUCCESS;

	/*===== Model Loading =====*/
	{
		//load vertex buffer and textures
		VulkanHelper::LoadGLTFFile(GAPI._VulkanUploader, "../../../media/Duck/Duck.gltf", _Model);

		{

			//the sampler and image to sample texture
			VkDescriptorSetLayoutBinding modelBindings[1] =
			{
				{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
			};

			VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _ModelDescriptors, modelBindings, 1);
		}

		VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _ModelDescriptors, _Model._Textures.Nb());

		for (uint32_t i = 0; i < _Model._Materials.Nb(); i++)
		{
			//the order of the textures in the material need to be ALBEDO, METAL-ROUGH, then NORMAL to actually work...
			for (uint32_t j = 0; j < _Model._Materials[i]._Textures.Nb(); j++)
			{
				//describing our combined sampler
				VkDescriptorImageInfo samplerInfo{};
				samplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				samplerInfo.imageView = _Model._Materials[i]._Textures[j]._ImageView;
				samplerInfo.sampler = _Model._Materials[i]._Textures[j]._Sampler;

				VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _ModelDescriptors, _Model._Materials[i]._Textures[j]._ImageView, _Model._Materials[i]._Textures[j]._Sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, j, i);

			}
			_Model._Materials[i]._TextureDescriptors = _ModelDescriptors._DescriptorSets[i];
		}
	}
}

//Creates the GPU BUffer necessary resources 
void DefferedRendering::PrepareGBufferProps(class GraphicsAPIManager& GAPI)
{
	VkResult result = VK_SUCCESS;

	{
		/*===== GPU Buffer INPUT ======*/

		//the uniform buffers for the models
		VkDescriptorSetLayoutBinding uniformBindings[1] =
		{
			{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
		};

		VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _GBufferDescriptors, uniformBindings, 1);


		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		VkDescriptorSetLayout layouts[2] = { _GBufferDescriptors._DescriptorLayout , _ModelDescriptors._DescriptorLayout };
		pipelineLayoutInfo.pSetLayouts = layouts;
		pipelineLayoutInfo.setLayoutCount = 2;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_GBUfferLayout));
	}

	{
		/*===== GBuffer OUTPUT ======*/

		//describing the format of the output (our framebuffers)
		VkAttachmentDescription attachements[3] =
		{
			//flags, format, samples, loadOp, storeOp, stencilLoadOp, stencilStoreOp, inialLayout, finalLayout
			{0, VK_FORMAT_R8G8B8A8_UNORM,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{0, VK_FORMAT_R32G32B32A32_SFLOAT,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
			{0, VK_FORMAT_R32G32B32A32_SFLOAT,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
		};

		/*===== DEPTH BUFFER CONFIG =====*/

		//describing the format of the depthbuffer
		VkAttachmentDescription depthBufferAttachment{};
		depthBufferAttachment.format = VK_FORMAT_D32_SFLOAT;
		depthBufferAttachment.samples = VK_SAMPLE_COUNT_1_BIT;//one pixel will have exactly one calculation done
		depthBufferAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//we'll make the pipeline clear the depth buffer
		depthBufferAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;//we just want the rasterizer to use it
		depthBufferAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthBufferAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthBufferAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//we write onto the entire vewport so it will be completely replaced, what was before does not interests us
		depthBufferAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;//we just want to use it as depth buffer.

		VulkanHelper::CreatePipelineOutput(GAPI._VulkanUploader, _GBUfferPipelineOutput, attachements, 3, depthBufferAttachment);

		//depth stencil clear value
		VulkanHelper::SetClearValue(_GBUfferPipelineOutput, { 0.2f,0.2f,0.2f,1.0f }, 0);
		VulkanHelper::SetClearValue(_GBUfferPipelineOutput, { 1.0f, 0 }, 3);
	}

	/* Pipeline Creation */

	CreateModelRenderPipeline(GAPI, _GBufferPipeline, _GBUfferLayout, _GBUfferPipelineOutput, _GBufferShaders);
}

void DefferedRendering::PrepareDefferedPassProps(class GraphicsAPIManager& GAPI)
{
	VkResult result = VK_SUCCESS;

	{
		/*===== Deffered OUTPUT ======*/

		//describing the format of the output (our framebuffers)
		VkAttachmentDescription attachements[1] =
		{
			//flags, format, samples, loadOp, storeOp, stencilLoadOp, stencilStoreOp, inialLayout, finalLayout
			{0, GAPI._VulkanSurfaceFormat.format,  VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}
		};

		VulkanHelper::CreatePipelineOutput(GAPI._VulkanUploader, _DefferedPipelineOutput, attachements, 1, {});
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		/*===== Deffered INPUT ======*/

		// a very basic sampler will be fine
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		vkCreateSampler(GAPI._VulkanDevice, &samplerInfo, nullptr, &_DefferedSampler);

		//the GPU Buffer images
		VkDescriptorSetLayoutBinding layoutBindings[4] =
		{
			//binding, descriptorType, descriptorCount, stageFlags, immutableSampler
			{0, VK_DESCRIPTOR_TYPE_SAMPLER , 1, VK_SHADER_STAGE_FRAGMENT_BIT, &_DefferedSampler},
			{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
			{2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
			{3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}
		};

		VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, layoutBindings, 4);

		//the GPU Buffer images
		VkDescriptorSetLayoutBinding compositinglayout =
		{
			//binding, descriptorType, descriptorCount, stageFlags, immutableSampler
			0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr
		};

		VulkanHelper::CreatePipelineDescriptor(GAPI._VulkanUploader, _DefferedCompositingDescriptors, &compositinglayout, 1);

		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		VkDescriptorSetLayout layouts[2] = { _DefferedDescriptors._DescriptorLayout , _DefferedCompositingDescriptors._DescriptorLayout };
		pipelineLayoutInfo.pSetLayouts = layouts;
		pipelineLayoutInfo.setLayoutCount = 2;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_DefferedLayout));
	}

	/* Pipeline Creation */

	CreateFullscreenCopyPipeline(GAPI, _DefferedPipeline, _DefferedLayout, _DefferedPipelineOutput, _DefferedShaders);
}

void DefferedRendering::CreateModelRenderPipeline(class GraphicsAPIManager& GAPI, VkPipeline& Pipeline, const VkPipelineLayout& PipelineLayout, const VulkanHelper::PipelineOutput& PipelineOutput, const List<VulkanHelper::ShaderScripts>& Shaders)
{
	VkResult result = VK_SUCCESS;

	/*===== SHADER ======*/

	//the shader stages we'll give to the pipeline
	MultipleScopedMemory<VkPipelineShaderStageCreateInfo> shaderStages;
	VulkanHelper::MakePipelineShaderFromScripts(shaderStages, Shaders);

	/*===== VERTEX INPUT ======*/

	VkVertexInputBindingDescription inputBindings[3] =
	{
		//binding, stride, inputrate
		{0, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX},//pos
		{1, sizeof(vec2), VK_VERTEX_INPUT_RATE_VERTEX},//texcoords
		{2, sizeof(vec3), VK_VERTEX_INPUT_RATE_VERTEX}//normals
	};

	VkVertexInputAttributeDescription inputAttribute[3] =
	{
		//location, binding, format, offset
		{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},//pos ; vec3
		{1, 1, VK_FORMAT_R32G32_SFLOAT, 0},//texcoords ; vec2
		{2, 2, VK_FORMAT_R32G32B32_SFLOAT, 0}//normals ; vec3
	};


	//the vertex input for the input assembly stage.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount	= 3;
	vertexInputInfo.pVertexBindingDescriptions		= inputBindings;
	vertexInputInfo.vertexAttributeDescriptionCount = 3;
	vertexInputInfo.pVertexAttributeDescriptions	= inputAttribute;

	//the description of the input assembly stage. by the fact we won't have any vertex, the input assembly will basically only give the indices (basically 0,1,2 if we ask for a draw command of three vertices)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType		= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	/*===== VIEWPORT/SCISSORS  =====*/

	//as window can be resized, I set viewport and scissors as dynamic so it is always the size of the output window
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount	= 2;
	VkDynamicState viewportState[2]		= { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	dynamicStateInfo.pDynamicStates		= viewportState;

	//setting our starting viewport
	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports	= &PipelineOutput._OutputViewport;
	viewportStateInfo.scissorCount	= 1;
	viewportStateInfo.pScissors		= &PipelineOutput._OutputScissor;

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

	//MSAA would be way overkill for what we are doing, so just disabling it
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	/*===== FRAMEBUFFER BLEND ======*/

	//description of colour blending and writing configuration : here we write every component of the output, but don't do alpha blending or other stuff
	VkPipelineColorBlendAttachmentState colorBlendAttachment [3]
	{
		//blendEnable, srcColorBlendFactor, dstColorBlendFactor, colorBlendOp, srcAlphaBlendFactor, dstAlphaBlendFactor, alphaBlendOp, colorWriteMask
		{VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},//colorBuffer
		{VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT},//posBuffer
		{VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}//normalBuffer

	};

	//description of colour blending :  here it is a simple write and discard old.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable		= VK_FALSE;
	colorBlending.attachmentCount	= PipelineOutput._OutputColorAttachement.Nb();
	colorBlending.pAttachments		= colorBlendAttachment;

	/*===== DEPTH TEST ======*/
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType				= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable	= VK_TRUE;
	depthStencil.depthWriteEnable	= VK_TRUE;
	depthStencil.depthCompareOp		= VK_COMPARE_OP_LESS;

	/*===== PIPELINE ======*/

	//finally creating our pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//fill up the pipeline description
	pipelineInfo.pVertexInputState		= &vertexInputInfo;
	pipelineInfo.pInputAssemblyState	= &inputAssembly;
	pipelineInfo.pViewportState			= &viewportStateInfo;
	pipelineInfo.pRasterizationState	= &rasterizer;
	pipelineInfo.pMultisampleState		= &multisampling;
	pipelineInfo.pDepthStencilState		= &depthStencil;
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= &dynamicStateInfo;
	pipelineInfo.pStages				= *shaderStages;
	pipelineInfo.stageCount				= Shaders.Nb();
	pipelineInfo.layout					= PipelineLayout;
	pipelineInfo.renderPass				= PipelineOutput._OutputRenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &Pipeline));
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

	//as window can be resized, I set viewport and scissors as dynamic so it is always the size of the output window
	VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
	dynamicStateInfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount	= 2;
	VkDynamicState viewportState[2]		= { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	dynamicStateInfo.pDynamicStates		= viewportState;

	//setting our starting viewport
	VkPipelineViewportStateCreateInfo viewportStateInfo{};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports	= &PipelineOutput._OutputViewport;
	viewportStateInfo.scissorCount	= 1;
	viewportStateInfo.pScissors		= &PipelineOutput._OutputScissor;


	/*==== RASTERIZER =====*/

	//Here, our rasterizer actually does the conversion from the vertex created inthe vertex shader and pixel coordinates\
	//it is the same as always, and frankly does not do much
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.polygonMode		= VK_POLYGON_MODE_FILL;
	rasterizer.cullMode			= VK_CULL_MODE_NONE;
	rasterizer.frontFace		= VK_FRONT_FACE_CLOCKWISE;
	rasterizer.lineWidth		= 1.0f;

	/*===== MSAA =====*/

	//MSAA would be way overkill for what wee are doing, so just disabling it
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable	= VK_FALSE;
	multisampling.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

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
	colorBlending.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable		= VK_FALSE;
	colorBlending.attachmentCount	= 1;
	colorBlending.pAttachments		= &colorBlendAttachment;

	/*===== PIPELINE ======*/

	//finally creating our pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	//fill up the pipeline description
	pipelineInfo.pVertexInputState		= &vertexInputInfo;
	pipelineInfo.pInputAssemblyState	= &inputAssembly;
	pipelineInfo.pViewportState			= &viewportStateInfo;
	pipelineInfo.pRasterizationState	= &rasterizer;
	pipelineInfo.pMultisampleState		= &multisampling;
	pipelineInfo.pDepthStencilState		= nullptr;
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= &dynamicStateInfo;
	pipelineInfo.pStages				= *shaderStages;
	pipelineInfo.stageCount				= Shaders.Nb();
	pipelineInfo.layout					= PipelineLayout;
	pipelineInfo.renderPass				= PipelineOutput._OutputRenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &Pipeline));
}


void DefferedRendering::PrepareGBufferScripts(GraphicsAPIManager& GAPI)
{
	//define vertex shader
	const char* g_buffer_vertex_shader =
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
				const vec4 modelPos = (model * vec4(positions, 1.0));
				gl_Position =  proj * view * modelPos;
				vertPos = modelPos.xyz;
				outTexCoord = inTexCoord;
				outNormal = normalize(( model * vec4(inNormal,0.0)).xyz);

			})";

	const char* g_buffer_fragment_shader =
		R"(#version 450
		layout(location = 0) in vec3 vertPos;
		layout(location = 1) in vec2 outTexCoord;
		layout(location = 2) in vec3 outNormal;

		layout(location = 0) out vec4 colorBuffer;
		layout(location = 1) out vec4 posBuffer;
		layout(location = 2) out vec4 normalBuffer;


		layout(set = 1, binding = 0) uniform sampler2D albedo;
		//layout(binding = 1, set = 1) uniform sampler2D pbr;
		//layout(binding = 1, set = 2) uniform sampler2D normal;


		void main()
		{
			colorBuffer = vec4(texture(albedo,outTexCoord).rgb,1.0);
			posBuffer = vec4(vertPos,1.0);
			normalBuffer = vec4(outNormal,1.0);
		}
		)";

	{
		VulkanHelper::ShaderScripts Script;

		//add vertex shader
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_VERTEX_BIT, g_buffer_vertex_shader, "GBuffer Pass Vertex"))
			_GBufferShaders.Add(Script);

		//add fragment shader
		if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_FRAGMENT_BIT, g_buffer_fragment_shader, "GBuffer Pass Frag"))
			_GBufferShaders.Add(Script);
	}
}

void DefferedRendering::PrepareCompositingScripts(GraphicsAPIManager& GAPI)
{
	//define vertex shader
	const char* deffered_vertex_shader =
		R"(#version 450
			layout(location = 0) out vec2 uv;

			void main()
			{
				uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
				gl_Position =  vec4(uv * 2.0f + -1.0f, 0.0f, 1.0f);

			})";

	const char* deffered_fragment_shader =
		R"(#version 450
		layout(location = 0) in vec2 uv;
		layout(location = 0) out vec4 outColor;

		layout(set = 0, binding = 0) uniform sampler pointSampler;
		layout(set = 0, binding = 1) uniform texture2D colorBuffer;
		layout(set = 0, binding = 2) uniform texture2D posBuffer;
		layout(set = 0, binding = 3) uniform texture2D normalBuffer;

		layout(set = 1, binding = 0) uniform CompositingBuffer
		{
			//the position of the camera for the light
			vec3	cameraPos;
			//an index allowing to change the output to debug frames
			uint	debugIndex;

			//directionnal Light direction
			vec3	directionalDir;
			//light padding
			float padding;
			//the color of the directional light
			vec3	directionnalColor;
			//light padding
			float padding2;

			//cel Shading Diffuse Step
			vec2	celDiffuseStep;
			//cel shading spec Step
			vec2	celSpecStep;

			//specular Glossiness
			float	specGlossiness;
			//the ambient occlusion
			float	ambientOcclusion;
		};

		//gets the "intensity" of the pixel, basically computing the length
		float intensity(vec3 vec) {
			return sqrt(dot(vec,vec));
		}

		//sobel filter coming from the wikipedia definition 
		float sobel(texture2D textureToSample, vec2 uv)
		{
			float x = 0;
			float y = 0;

			vec2 offset = vec2(1.0,1.0);

			float a  = 1.0f;
			float b = 2.0f;

			x += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(-offset.x, -offset.y)).rgb) * -a;
			x += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(-offset.x,         0)).rgb) * -b;
			x += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(-offset.x,  offset.y)).rgb) * -a;
								   
			x += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(offset.x, -offset.y)).rgb)  * a;
			x += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(offset.x,			0)).rgb)  * b;
			x += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2( offset.x,  offset.y)).rgb) * a;
								   
			y += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(-offset.x, -offset.y)).rgb) * -a;
			y += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(0,		 -offset.y)).rgb) * -b;	   
			y += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2( offset.x, -offset.y)).rgb) * -a;
								   
			y += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(-offset.x, offset.y)).rgb ) * a;
			y += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2(0,		 offset.y)).rgb ) * b;
			y += intensity(texture(sampler2D(textureToSample,pointSampler), uv + vec2( offset.x,  offset.y)).rgb) * a;

			return sqrt(x * x + y * y);
		}

		void main()
		{
			const vec3 pos = texture(sampler2D(posBuffer,pointSampler),uv).rgb;
			const vec3 normal = texture(sampler2D(normalBuffer,pointSampler),uv).rgb;
			const vec3 color = texture(sampler2D(colorBuffer,pointSampler),uv).rgb;

			const float outline = sobel(normalBuffer, uv);

			if (dot(normal,normal) == 0.0)
			{
				outColor = vec4(color,1.0);
				return;
			}


			vec3 lightDir = normalize(directionalDir);
			vec3 viewDir = normalize(pos - cameraPos);

			vec3 halfDir	= normalize(viewDir+lightDir);
			float specAngle = max(dot(halfDir,normal), 0.0);
			vec3 specular	=  directionnalColor * smoothstep(celSpecStep.x, celSpecStep.y,pow(specAngle,specGlossiness* specGlossiness));
			vec3 diffuse	= directionnalColor * smoothstep(celDiffuseStep.x,celDiffuseStep.y,dot(lightDir, normal));

			//the compositing input
			if (debugIndex == 0)
			{
				//light = specular + diffuse + ambient occlusion
				outColor = vec4(color * (diffuse + specular + ambientOcclusion),1.0);
			}
			else if (debugIndex == 1)
			{
				outColor = vec4(pos,1.0);
			}
			else if (debugIndex == 2)
			{
				outColor = vec4(normal*0.5+0.5,1.0);
			}
			else if (debugIndex == 3)
			{
				outColor = vec4(color,1.0);
			}
			else if (debugIndex == 4)
			{
				outColor = vec4(diffuse,1.0);
			}
			else if (debugIndex == 5)
			{
				outColor = vec4(specular,1.0);
			}
			else if (debugIndex == 6)
			{
				outColor = vec4(vec3(outline),1.0);
			}
		}
		)";


		{
			VulkanHelper::ShaderScripts Script;

			//add vertex shader
			if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_VERTEX_BIT, deffered_vertex_shader, "Deffered Pass Vertex"))
				_DefferedShaders.Add(Script);

			//add fragment shader
			if (CreateVulkanShaders(GAPI._VulkanUploader, Script, VK_SHADER_STAGE_FRAGMENT_BIT, deffered_fragment_shader, "Deffered Pass Frag"))
				_DefferedShaders.Add(Script);
		}
}

void DefferedRendering::PrepareVulkanScripts(GraphicsAPIManager& GAPI)
{
	PrepareGBufferScripts(GAPI);
	PrepareCompositingScripts(GAPI);
}

void DefferedRendering::Prepare(class GraphicsAPIManager& GAPI)
{
	//a "zero init" of the transform values
	_ObjData._Trs.scale = vec3{ 1.0f, 1.0f, 1.0f };
	_ObjData._Trs.pos = vec3{ 0.0f, 0.0f, 0.0f };

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


void DefferedRendering::ResizeGBufferResources(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//clear the fullscreen images buffer
	if (_GBuffers != nullptr)
	{
		ClearFrameBuffer(GAPI._VulkanDevice, _GBuffers[0]);
		ClearFrameBuffer(GAPI._VulkanDevice, _GBuffers[1]);
		ClearFrameBuffer(GAPI._VulkanDevice, _GBuffers[2]);
		_GBuffers.Clear();
	}

	VulkanHelper::ReleaseFrameBuffer(GAPI._VulkanDevice, _GBUfferPipelineOutput);
	VulkanHelper::ReleaseDescriptor(GAPI._VulkanDevice, _GBufferDescriptors);

	/*===== Pipeline OUTPUT ======*/

	//reallocate the fullscreen GBuffer and present iamges render target with the new number of available images
	VulkanHelper::AllocateFrameBuffer(_GBUfferPipelineOutput, GAPI._vk_width, GAPI._vk_height, GAPI._nb_vk_frames);

	/*===== DESCRIPTORS ======*/

	VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _GBufferDescriptors, GAPI._nb_vk_frames);

	/*===== UNIFORM BUFFERS ======*/

	//recreate the uniform buffer
	CreateUniformBufferHandle(GAPI._VulkanUploader, _GBUfferUniformBuffer, GAPI._nb_vk_frames, sizeof(UniformBuffer));

	/*===== GPU SIDE IMAGE BUFFERS ======*/

	_GBuffers.Alloc(3);
	CreateFrameBuffer(GAPI._VulkanUploader, _GBuffers[0], _GBUfferPipelineOutput, 0);
	CreateFrameBuffer(GAPI._VulkanUploader, _GBuffers[1], _GBUfferPipelineOutput, 1);
	CreateFrameBuffer(GAPI._VulkanUploader, _GBuffers[2], _GBUfferPipelineOutput, 2);

	/*===== CREATING & LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{

		//make our gbuffer linked to the framebuffers
		VkImageView g_buffers[4] = { _GBuffers[0]._ImageViews[i], _GBuffers[1]._ImageViews[i], _GBuffers[2]._ImageViews[i], GAPI._VulkanDepthBufferViews[i] };
		VulkanHelper::SetFrameBuffer(GAPI._VulkanUploader, _GBUfferPipelineOutput, g_buffers, i);

		//set our uniform buffer for our gbuffer pass
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _GBufferDescriptors, _GBUfferUniformBuffer._GPUBuffer[i], 0, sizeof(UniformBuffer), 0, i);
	}
}

void DefferedRendering::ResizeDefferedPassResources(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	VulkanHelper::ReleaseFrameBuffer(GAPI._VulkanDevice, _DefferedPipelineOutput);
	VulkanHelper::ReleaseDescriptor(GAPI._VulkanDevice, _DefferedDescriptors);
	VulkanHelper::ReleaseDescriptor(GAPI._VulkanDevice, _DefferedCompositingDescriptors);

	/*===== Pipeline OUTPUT ======*/

	//reallocate the fullscreen GBuffer and present iamges render target with the new number of available images
	VulkanHelper::AllocateFrameBuffer(_DefferedPipelineOutput, GAPI._vk_width, GAPI._vk_height, GAPI._nb_vk_frames);

	/*===== DESCRIPTORS ======*/

	VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _DefferedCompositingDescriptors, GAPI._nb_vk_frames);
	VulkanHelper::AllocateDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, GAPI._nb_vk_frames);
	
	/*===== UNIFORM BUFFERS ======*/

	//recreate the uniform buffer
	CreateUniformBufferHandle(GAPI._VulkanUploader, _DefferedUniformBuffer, GAPI._nb_vk_frames, sizeof(CompositingBuffer));

	/*===== CREATING & LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//make our framebuffer linked to the swapchain back buffers
		VulkanHelper::SetFrameBuffer(GAPI._VulkanUploader, _DefferedPipelineOutput, &GAPI._VulkanBackColourBuffers[i], i);

		//wrinting our newly created imatge view in descriptor.
		//sampler is immutable so no need to send it.
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, _GBuffers[0]._ImageViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, i);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, _GBuffers[1]._ImageViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 2, i);
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _DefferedDescriptors, _GBuffers[2]._ImageViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 3, i);

		//also set our uniform buffer for our deffered/compositing pass
		VulkanHelper::UploadDescriptor(GAPI._VulkanUploader, _DefferedCompositingDescriptors, _DefferedUniformBuffer._GPUBuffer[i], 0, sizeof(CompositingBuffer), 0, i);
	}
}

void DefferedRendering::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	ResizeGBufferResources(GAPI, width, height, old_nb_frames);
	ResizeDefferedPassResources(GAPI, width, height, old_nb_frames);
}



void DefferedRendering::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);

}

/*===== Act =====*/

void DefferedRendering::Act(struct AppWideContext& AppContext)
{
	//UI update
	if (SceneCanShowUI(AppContext))
	{
		//Deffered Buffer debug visualization
		_ObjData._ChangedFlag| ImGui::SliderInt("Debug Frame", (int*)&_CompositingBuffer._debugIndex, 0, 6);

		//Object Transform
		if (ImGuiHelper::TransformUI("Object", _ObjData._Trs, _ObjData._ChangedFlag))
		{
			//Realtime control
			_ObjData._ChangedFlag |= ImGui::SliderFloat("Object Rot Speed", &_ObjData._ObjRotSpeed, 0.0f, 100.0f, "%0.01f");
		}

		//light edit
		if (ImGuiHelper::LightUI("Directionnal Light", _CompositingBuffer._dirLight, _ObjData._ChangedFlag))
		{
			_ObjData._ChangedFlag |= ImGui::SliderFloat("Ambient Occlusion", &_CompositingBuffer._ambientOcclusion, 0.0f, 1.0f);
		}

		//shading params
		ImGuiHelper::CelParamsUI("Cel Shading Parameters", _CompositingBuffer._celParams, _ObjData._ChangedFlag);
	}

	_CompositingBuffer._cameraPos = AppContext.camera_pos;

	_ObjData._Trs.rot.y += _ObjData._ObjRotSpeed * AppContext.delta_time;


	//it will change every frame
	{
		_UniformBuffer._proj = perspective_proj(_GBUfferPipelineOutput._OutputScissor.extent.width, _GBUfferPipelineOutput._OutputScissor.extent.height, AppContext.fov, AppContext.near_plane, AppContext.far_plane);
		_UniformBuffer._view = AppContext.view_mat;
		_UniformBuffer._model = TransformToMat(_ObjData._Trs);

	}
}

/*===== Show =====*/


void DefferedRendering::BindPass(GAPIHandle& GAPIHandle, const VkPipeline& Pipeline,
	const VulkanHelper::PipelineOutput& PipelineOutput)
{
	const VkCommandBuffer& commandBuffer = GAPIHandle.GetCurrentVulkanCommand();

	//begin render pass onto whole screen
	{
		//Set output and output settings for this render pass.
		VkRenderPassBeginInfo info = {};
		info.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass		= PipelineOutput._OutputRenderPass;
		info.framebuffer	= PipelineOutput._OuputFrameBuffer[GAPIHandle._vk_frame_index];
		info.renderArea		= PipelineOutput._OutputScissor;
		info.clearValueCount = PipelineOutput._OutputClearValue.Nb();
		info.pClearValues	= *PipelineOutput._OutputClearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//bind pipeline, descriptor, viewport and scissors ...
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &PipelineOutput._OutputViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &PipelineOutput._OutputScissor);
}

void DefferedRendering::DrawModel(GAPIHandle& GAPIHandle, const VulkanHelper::Model& model, const VkPipelineLayout& PipelineLayout, const VulkanHelper::PipelineDescriptors& descriptors)
{
	//get the command buffer for this draw
	const VkCommandBuffer& commandBuffer = GAPIHandle.GetCurrentVulkanCommand();

	//draw all meshes, as model may be composed of multiple meshes
	for (uint32_t i = 0; i < model._Meshes.Nb(); i++)
	{
		//first bind the "material", basically the three compibined sampler descriptors ...
		if (model._Materials.Nb() > 0)
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 1, 1, &descriptors._DescriptorSets[model._material_index[i]], 0, nullptr);

		//... then bind the vertex buffer as described in the input layout of the pipeline ...
		vkCmdBindVertexBuffers(commandBuffer, 0, 3, model._Meshes[i]._VertexBuffers, (VkDeviceSize*)model._Meshes[i]._vertex_offsets);
		//... and the index buffers associated with the vertex buffers ...
		vkCmdBindIndexBuffer(commandBuffer, model._Meshes[i]._Indices, model._Meshes[i]._indices_offset, VK_INDEX_TYPE_UINT32);
		//... before finally drawing, following the index buffer.
		vkCmdDrawIndexed(commandBuffer, model._Meshes[i]._indices_nb, 1, 0, 0, 0);
	}
}

void DefferedRendering::StartGBuffer(GAPIHandle& GAPIHandle)
{
	//get the command buffer for this draw
	const VkCommandBuffer& commandBuffer = GAPIHandle.GetCurrentVulkanCommand();

	BindPass(GAPIHandle, _GBufferPipeline, _GBUfferPipelineOutput);
}

void DefferedRendering::EndGBuffer(GAPIHandle& GAPIHandle)
{
	//end G buffer pass
	vkCmdEndRenderPass(GAPIHandle.GetCurrentVulkanCommand());
}

void DefferedRendering::DrawCompositingPass(GAPIHandle& GAPIHandle)
{
	//get the command buffer for this draw
	const VkCommandBuffer& commandBuffer = GAPIHandle.GetCurrentVulkanCommand();

	//start deffered pass
	BindPass(GAPIHandle, _DefferedPipeline, _DefferedPipelineOutput);

	//the buffer will change every frame as the object rotates every frame
	memcpy(_DefferedUniformBuffer._CPUMemoryHandle[GAPIHandle._vk_current_frame], (void*)&_CompositingBuffer, sizeof(CompositingBuffer));
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _DefferedLayout, 1, 1, &_DefferedCompositingDescriptors._DescriptorSets[GAPIHandle._vk_current_frame], 0, nullptr);

	//bind all the buffers
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _DefferedLayout, 0, 1, &_DefferedDescriptors._DescriptorSets[GAPIHandle._vk_frame_index], 0, nullptr);

	// ... then draw !
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	//end writing in our backbuffer
	vkCmdEndRenderPass(commandBuffer);
}

void DefferedRendering::Show(GAPIHandle& GAPIHandle)
{
	//to record errors
	VkResult result = VK_SUCCESS;

	//current frames resources
	const VkCommandBuffer& commandBuffer = GAPIHandle.GetCurrentVulkanCommand();
	const VkSemaphore& waitSemaphore = GAPIHandle.GetCurrentCanPresentSemaphore();
	const VkSemaphore& signalSemaphore = GAPIHandle.GetCurrentHasPresentedSemaphore();

	{
		//first, reset previous records
		VK_CALL_PRINT(vkResetCommandBuffer(commandBuffer, 0));

		//then open for record
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CALL_PRINT(vkBeginCommandBuffer(commandBuffer, &info));

	}


	for (uint32_t i = 0; i < 3; i++)
	{
		VulkanHelper::ImageMemoryBarrier(commandBuffer, _GBuffers[i]._Images[GAPIHandle._vk_frame_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	StartGBuffer(GAPIHandle);


	//the buffer will change every frame as the object rotates every frame
	memcpy(_GBUfferUniformBuffer._CPUMemoryHandle[GAPIHandle._vk_current_frame], (void*)&_UniformBuffer, sizeof(UniformBuffer));
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _GBUfferLayout, 0, 1, &_GBufferDescriptors._DescriptorSets[GAPIHandle._vk_current_frame], 0, nullptr);

	DrawModel(GAPIHandle, _Model, _GBUfferLayout, _ModelDescriptors);

	EndGBuffer(GAPIHandle);

	DrawCompositingPass(GAPIHandle);

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
	//clear model resrouces
	VulkanHelper::ClearModel(GAPI._VulkanDevice,_Model);
	VulkanHelper::ClearPipelineDescriptor(GAPI._VulkanDevice, _ModelDescriptors);
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _GBUfferUniformBuffer);
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _DefferedUniformBuffer);

	//Clear Shaders
	CLEAR_LIST(_GBufferShaders, _GBufferShaders.Nb(), VulkanHelper::ClearVulkanShader, GAPI._VulkanDevice);
	CLEAR_LIST(_DefferedShaders, _DefferedShaders.Nb(), VulkanHelper::ClearVulkanShader, GAPI._VulkanDevice);

	//clear descriptors
	vkDestroySampler(GAPI._VulkanDevice, _DefferedSampler, nullptr);
	VulkanHelper::ClearPipelineDescriptor(GAPI._VulkanDevice, _GBufferDescriptors);
	VulkanHelper::ClearPipelineDescriptor(GAPI._VulkanDevice, _DefferedDescriptors);
	VulkanHelper::ClearPipelineDescriptor(GAPI._VulkanDevice, _DefferedCompositingDescriptors);

	//clear framebuffers
	VulkanHelper::ClearPipelineOutput(GAPI._VulkanDevice, _GBUfferPipelineOutput);
	VulkanHelper::ClearPipelineOutput(GAPI._VulkanDevice, _DefferedPipelineOutput);
    if (_GBuffers != nullptr)
    {
        for (uint32_t i = 0; i < 3; i++)
        {
            ClearFrameBuffer(GAPI._VulkanDevice, _GBuffers[i]);
        }
		_GBuffers.Clear();
    }

	/* DEFFERED COMPOSITING */

	//destroy pipeline resources
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _GBUfferLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _GBufferPipeline, nullptr);
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _DefferedLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _DefferedPipeline, nullptr);
}
