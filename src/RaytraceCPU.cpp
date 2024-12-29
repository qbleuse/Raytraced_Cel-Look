#include "RaytraceCPU.h"

//helper incldue
#include "RaytraceCPUHelper.inl"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"

//include loader app
#include "tiny_obj_loader.h"

/*==== Prepare =====*/

void RaytraceCPU::PrepareVulkanScripts(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
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


	CreateVulkanShaders(GAPI._VulkanUploader, VertexShader, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raytrace Fullscreen Vertex");
	CreateVulkanShaders(GAPI._VulkanUploader, FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raytrace Fullscreen Frag");
}

void RaytraceCPU::PrepareVulkanProps(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
{
	VkResult result = VK_SUCCESS;

	/*===== SHADER ======*/

	//describe vertex shader stage
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = VertexShader;
	vertShaderStageInfo.pName = "main";

	//describe fragment shader stage
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = FragmentShader;
	fragShaderStageInfo.pName = "main";

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
	viewportStateInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports	= &_FullScreenViewport;
	viewportStateInfo.scissorCount	= 1;
	viewportStateInfo.pScissors		= &_FullScreenScissors;


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
		renderPassInfo.pSubpasses		= &subpass;
		VkSubpassDependency dependency{};
		dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VK_CALL_PRINT(vkCreateRenderPass(GAPI._VulkanDevice, &renderPassInfo, nullptr, &_FullScreenRenderPass));
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		// a very basic sampler will be fine
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		vkCreateSampler(GAPI._VulkanDevice, &samplerInfo, nullptr, &_FullScreenSampler);


		//the sampler and image to sample the CPU texture
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding			= 0;
		samplerLayoutBinding.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount	= 1;
		samplerLayoutBinding.stageFlags			= VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = &_FullScreenSampler;


		//the descriptor layout that include the immutable sampler and the local GPU image
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings	= &samplerLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_GPUImageDescriptorLayout));

	}

	{
		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts		= &_GPUImageDescriptorLayout;
		pipelineLayoutInfo.setLayoutCount	= 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_FullScreenLayout));
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
	pipelineInfo.pDepthStencilState		= nullptr;
	pipelineInfo.pColorBlendState		= &colorBlending;
	pipelineInfo.pDynamicState			= &dynamicStateInfo;
	pipelineInfo.layout					= _FullScreenLayout;
	pipelineInfo.renderPass				= _FullScreenRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_FullScreenPipeline));

	vkDestroyShaderModule(GAPI._VulkanDevice, FragmentShader, nullptr);
	vkDestroyShaderModule(GAPI._VulkanDevice, VertexShader, nullptr);
}



void RaytraceCPU::Prepare(GraphicsAPIManager& GAPI)
{
	//the shaders needed
	VkShaderModule VertexShader, FragmentShader;

	//compile the shaders here...
	PrepareVulkanScripts(GAPI, VertexShader, FragmentShader);
	//then create the pipeline
	PrepareVulkanProps(GAPI, VertexShader, FragmentShader);

	//Allocate for teh materil of a random scene (we are overallocating here as dieletrics will always have the same material)
	_Materials.Alloc(29);
	_Materials[0] = new diffuse( vec4{ 0.5f,0.5f,0.5f, 1.0f} );//the ground material
	_Materials[1] = new diffuse( vec4{ 0.4f, 0.2f, 0.1f, 1.0f });//a lambertian diffuse example
    _Materials[2] = new dieletrics( vec4{ 1.0f,1.0f,1.0f,1.0f }, 1.50f);//a dieletric example (this is a glass sphere)
	_Materials[3] = new metal( vec4{ 0.7f, 0.6f, 0.5f, 1.0f });//a metal example

	//Allocate for a random scene (all the space allocated will be used)
	_Scene.Alloc(29);
	_Scene[0] = new sphere(vec3{ 0.0f, -1000.0f, 0.0f }, 1000.0f, _Materials[0]);//the ground
	_Scene[1] = new sphere(vec3{ 0.0f, 5.0f, 0.0f }, 5.0f, _Materials[1]);//a lambertian diffuse example
	_Scene[2] = new sphere(vec3{ -10.0f, 5.0f, 0.0f }, 5.0f, _Materials[2]);//a dieletric example (this is a glass sphere)
    _Scene[3] = new sphere(vec3{ 10.0f, 5.0f, 0.0f }, 5.0f, _Materials[3]);//a metal example

	//we already have the example material, so further material will start after
	uint32_t matNb = 4;
	//we already have example sphere, so further objects will be created after
	uint32_t sphereNb = 4;
	//we create sphere at random place with random materials for the demo
	for (int32_t x = -2; x <= 2; x++)
	{
		for (int32_t z = -2; z <= 2; z++)
		{
			float choose_mat = randf();
			vec3 sphereCenter = vec3{(static_cast<float>(x)+ randf(-1.0f,1.0f)) * 10.0f, 1.5f, (static_cast<float>(z) + randf(-1.0f,1.0f)) * 10.0f };
	
			if (choose_mat < 0.8f)
			{
				_Materials[matNb++] = new diffuse(vec4{randf(),randf(),randf(),1.0f});
				_Scene[sphereNb++] = new sphere(sphereCenter, 1.5f, _Materials[matNb - 1]);
			}
			else if (choose_mat < 0.95f)
			{
				_Materials[matNb++] = new metal(vec4{ randf(0.5f,1.0f),randf(0.5f,1.0f),randf(0.5f,1.0f),1.0f });
				_Scene[sphereNb++] = new sphere(sphereCenter, 1.5f, _Materials[matNb - 1]);
			}
			else
			{
				_Scene[sphereNb++] = new sphere(sphereCenter, 1.5f, _Materials[2]);
			}
		}
	}
}


/*==== Resize =====*/

void RaytraceCPU::ResizeVulkanResource(GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//clear the fullscreen images buffer
	VK_CLEAR_ARRAY(_ImageCopyBuffer, old_nb_frames, vkDestroyBuffer, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_GPULocalImageViews, old_nb_frames, vkDestroyImageView, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_GPULocalImageBuffers, old_nb_frames, vkDestroyImage, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_FullScreenOutput, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
	_MappedCPUImage.Clear();
	_RaytracedImage.Clear();

	//free the allocated memory for the fullscreen images
	vkFreeMemory(GAPI._VulkanDevice, _GPULocalImageMemory, nullptr);
	vkFreeMemory(GAPI._VulkanDevice, _ImageCopyMemory, nullptr);
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _GPUImageDescriptorPool, nullptr);


	/*===== VIEWPORT AND SCISSORS ======*/

	//filling up the viewport and scissors
	_FullScreenViewport.width	= (float)GAPI._vk_width;
	_FullScreenViewport.height	= (float)GAPI._vk_height;
	_FullScreenViewport.maxDepth = 1.0f;
	_FullScreenViewport.minDepth = 0.0f;
	_FullScreenViewport.x		= 0.0f;
	_FullScreenViewport.y		= 0.0f;
	_FullScreenScissors.extent = { (uint32_t)GAPI._vk_width, (uint32_t)GAPI._vk_height };

	/*===== CPU SIDE IMAGE BUFFERS ======*/

	//allocating a new space for the CPU image
	_RaytracedImage.Alloc(GAPI._vk_width * GAPI._vk_height);

	//reallocate the fullscreen image buffers with the new number of available images
	_FullScreenOutput.Alloc(GAPI._nb_vk_frames);
	_ImageCopyBuffer.Alloc(GAPI._nb_vk_frames);
	_GPULocalImageViews.Alloc(GAPI._nb_vk_frames);
	_GPULocalImageBuffers.Alloc(GAPI._nb_vk_frames);
	_MappedCPUImage.Alloc(GAPI._nb_vk_frames);


	/*===== DESCRIPTORS ======*/

	//recreate the descriptor pool
	{
		//there is only sampler and image in the descriptor pool
		VkDescriptorPoolSize poolUniformSize{};
		poolUniformSize.type			= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolUniformSize.descriptorCount = GAPI._nb_vk_frames;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 1;
		poolInfo.pPoolSizes		= &poolUniformSize;
		poolInfo.maxSets		= GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_GPUImageDescriptorPool))
	}

	//allocate the memory for descriptor sets
	{
		//describe a descriptor set for each of our framebuffer (a uniform bufer per frame)
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= _GPUImageDescriptorPool;
		allocInfo.descriptorSetCount	= GAPI._nb_vk_frames;

		//make a copy of our loayout for the number of frames needed
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_GPUImageDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//Allocate descriptor sets on CPU and GPU side
		_GPUImageDescriptorSets.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_GPUImageDescriptorSets));
	}

	/*===== GPU SIDE IMAGE BUFFERS ======*/

	// the size of the fullscreen image's buffer : a fullscreen rgba texture
	VkDeviceSize bufferSize = static_cast<VkDeviceSize>(_FullScreenViewport.width * _FullScreenViewport.height * sizeof(vec4));

	//allocate the memory for buffer and image
	{
		//allocate memory for buffer
		{
			VulkanHelper::CreateVulkanBufferAndMemory(GAPI._VulkanUploader, bufferSize * static_cast<VkDeviceSize>(GAPI._nb_vk_frames), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _ImageCopyBuffer[0], _ImageCopyMemory, 0, true);
			
			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(GAPI._VulkanDevice, _ImageCopyBuffer[0], &memRequirements);
			_ImageCopyBufferSize = memRequirements.size / static_cast<VkDeviceSize>(GAPI._nb_vk_frames);

			vkDestroyBuffer(GAPI._VulkanDevice, _ImageCopyBuffer[0], nullptr);
		}

		// creating the image object with data given in parameters
		VkImageCreateInfo imageInfo{};
		imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType		= VK_IMAGE_TYPE_2D;//we copy a fullscreen image
		imageInfo.format		= VK_FORMAT_R32G32B32A32_SFLOAT;//easier to manipulate (may be unsuppoorted by some GPUs)
		imageInfo.extent.width	= _FullScreenScissors.extent.width;
		imageInfo.extent.height = _FullScreenScissors.extent.height;
		imageInfo.extent.depth	= 1;
		imageInfo.mipLevels		= 1u;
		imageInfo.arrayLayers	= 1u;
		imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL;//will only load images from buffers, so no need to change this
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//same this can be changed later and each _Scene will do what they need on their own
		imageInfo.usage			= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;//this is slighlty more important as we may use image as deffered rendering buffers, so made it available
		imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;//we have to command queues : one for the app, one for the UI. the UI shouldn't use iamge we create.
		imageInfo.samples		= VK_SAMPLE_COUNT_1_BIT;//why would you want more than one sample in a simple app ?
		imageInfo.flags = 0; // Optional

		VK_CALL_PRINT(vkCreateImage(GAPI._VulkanDevice, &imageInfo, nullptr, &_GPULocalImageBuffers[0]));

		//getting the necessary requirements to create our image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(GAPI._VulkanDevice, _GPULocalImageBuffers[0], &memRequirements);
		_GPULocalImageBufferSize = memRequirements.size;

		//trying to find a matching memory type between what the app wants and the device's limitation.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size * GAPI._nb_vk_frames;//we want multiple frames
		allocInfo.memoryTypeIndex = VulkanHelper::GetMemoryTypeFromRequirements(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements,GAPI._VulkanUploader._MemoryProperties);

		//allocating and associate the memory to our image.
		VK_CALL_PRINT(vkAllocateMemory(GAPI._VulkanDevice, &allocInfo, nullptr, &_GPULocalImageMemory));

		vkDestroyImage(GAPI._VulkanDevice, _GPULocalImageBuffers[0], nullptr);
	}

	/*===== FRAMEBUFFERS ======*/

	//describing the framebuffer to the swapchain
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType		= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass	= _FullScreenRenderPass;
	framebufferInfo.width		= GAPI._vk_width;
	framebufferInfo.height		= GAPI._vk_height;
	framebufferInfo.layers		= 1;

	/*===== CREATING & LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//make our framebuffer linked to the swapchain back buffers
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &GAPI._VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI._VulkanDevice, &framebufferInfo, nullptr, &_FullScreenOutput[i]))

		//create the buffer object that will be used to copy from CPU to GPU
		VulkanHelper::CreateVulkanBufferAndMemory(GAPI._VulkanUploader, _ImageCopyBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _ImageCopyBuffer[i], _ImageCopyMemory, _ImageCopyBufferSize * i, false);

		//create the image that will be used to write from texture to screen frame buffer
		VulkanHelper::CreateImage(GAPI._VulkanUploader, _GPULocalImageBuffers[i], _GPULocalImageMemory, _FullScreenScissors.extent.width, _FullScreenScissors.extent.height, 1, VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _GPULocalImageBufferSize * i, false);

		// changing the image to allow read from shader
		VkImageMemoryBarrier barrier{};
		barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout						= VK_IMAGE_LAYOUT_UNDEFINED;//from nothing
		barrier.newLayout						= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//to transfer dest
		barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.image							= _GPULocalImageBuffers[i];
		barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel	= 0;
		barrier.subresourceRange.levelCount		= 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount		= 1;
		barrier.srcAccessMask					= 0;// making the image accessible for ...
		barrier.dstAccessMask					= VK_ACCESS_SHADER_READ_BIT; // shaders

		//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
		vkCmdPipelineBarrier(GAPI._VulkanUploader._CopyBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		//create an image view associated with the GPU local Image to make it available in shader
		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType						= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.subresourceRange.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.format						= VK_FORMAT_R32G32B32A32_SFLOAT;
		viewCreateInfo.image						= _GPULocalImageBuffers[i];
		viewCreateInfo.subresourceRange.layerCount	= 1;
		viewCreateInfo.subresourceRange.levelCount	= 1;
		viewCreateInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;

		VK_CALL_PRINT(vkCreateImageView(GAPI._VulkanDevice, &viewCreateInfo, nullptr, &_GPULocalImageViews[i]));

		//wrinting our newly created imatge view in descriptor.
		//sampler is immutable so no need to send it.
		VkDescriptorImageInfo imageViewInfo{};
		imageViewInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageViewInfo.imageView = _GPULocalImageViews[i];

		VkWriteDescriptorSet imageDescriptorWrite{};
		imageDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageDescriptorWrite.dstSet				= _GPUImageDescriptorSets[i];
		imageDescriptorWrite.dstBinding			= 0;
		imageDescriptorWrite.dstArrayElement	= 0;
		imageDescriptorWrite.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imageDescriptorWrite.descriptorCount	= 1;
		imageDescriptorWrite.pImageInfo			= &imageViewInfo;

		vkUpdateDescriptorSets(GAPI._VulkanDevice, 1, &imageDescriptorWrite, 0, nullptr);
	}
}

void RaytraceCPU::Resize(GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
	_ComputeHeap = MultipleSharedMemory<ray_compute>(_FullScreenScissors.extent.width * _FullScreenScissors.extent.height + _FullScreenScissors.extent.width * _FullScreenScissors.extent.height * _pixel_sample_nb);
	_need_refresh = true;
}


/*==== Act =====*/

void RaytraceCPU::DispatchSceneRay(AppWideContext& AppContext)
{
	//2D viewport values
	float windowHeight	= static_cast<float>(_FullScreenScissors.extent.height);
	float windowWidth	= static_cast<float>(_FullScreenScissors.extent.width);
	float aspectRatio	= windowWidth / windowHeight;

	//3D viewport and Camera values
	float focalLength			= 1.0f;
	float viewportHeight		= 2.0f * tanf(AppContext.fov * 0.5f) * focalLength;
	float viewportWidth			= viewportHeight * (windowWidth / windowHeight);
	const vec3& cameraCenter	= AppContext.view_mat.vector[3].xyz;

	mat4 viewMat = transpose(AppContext.view_mat);

	//The ray generation sample vectors
	vec3 viewportU = viewMat.vector[0].xyz * viewportWidth;
	vec3 viewportV = viewMat.vector[1].xyz * -viewportHeight;
	vec3 viewportW = viewMat.vector[2].xyz;
	vec3 pixelDeltaU = viewportU / windowWidth;
	vec3 pixelDeltaV = viewportV / windowHeight;

	//first sample dir and sample steps
	vec3 viewportUpperLeft	= cameraCenter - viewportW - (viewportU * 0.5f) - (viewportV * 0.5f);
	vec3 firstPixel			= viewportUpperLeft + ((pixelDeltaU + pixelDeltaV) * 0.5f);
	
	{
		//going over every pixel
		for (uint32_t h = 0; h < _FullScreenScissors.extent.height; h++)
			for (uint32_t w = 0; w < _FullScreenScissors.extent.width; w++)
			{
				//first get the pixel's "3D position", (we anti-aliase using random)
				vec3 pixelCenter = firstPixel + pixelDeltaU * (static_cast<float>(w) + randf() - 0.5f) 
												+ pixelDeltaV * (static_cast<float>(h) + randf() - 0.5f);
				//create a direction from the camera's position to the 3D viewport for this pixel
				vec3 rayDir = normalize(pixelCenter - cameraCenter);//normalizing is very slow, we'll still do it, but you may want to remove it

				ray pixelRay = ray{ pixelCenter, rayDir };

                _ComputeHeap[(h * _FullScreenScissors.extent.width) + w].launched = pixelRay;
                _ComputeHeap[(h * _FullScreenScissors.extent.width) + w].pixel = &_RaytracedImage[(h * _FullScreenScissors.extent.width) + w];
			}
		
		//clearing the current work and sending the new one
		_batch_fence.lock();
		_ComputeBatch.Clear();
		_ComputeBatch.PushBatch(_ComputeHeap, _FullScreenScissors.extent.width * _FullScreenScissors.extent.height);
		_batch_fence.unlock();
	}

	//pause the thread loop to add the new concurrent work
	AppContext.threadPool.Pause();
	if (!_is_moving)//if we are moving the old work are obsolete
		AppContext.threadPool.ClearJobs();

	//going over all the screen
	for (uint32_t i = 0; i < _FullScreenScissors.extent.width * _FullScreenScissors.extent.height;)
	{
		//asking for a Batch for the job to work on with the size user chose
		uint32_t computeNb				= _compute_per_frames;
		const RayBatch& computes_job	= _ComputeBatch.PopBatch(computeNb);

		if (computeNb > 0)
		{
			RayBatch newBatch{ computes_job.data,computes_job.offset,computes_job.nb };

			//adding the job without asking for immediate execution
			FirstContactRaytraceJob newJob(newBatch,*this);
			AppContext.threadPool.SilentAdd(newJob);
		}

		//adding the number of processed pixel
		i += computeNb;
	}
	//now asking for all thread to start working
	AppContext.threadPool.Resume();
}

void RaytraceCPU::Act(AppWideContext& AppContext)
{
	//UI update
	if (SceneCanShowUI(AppContext))
	{
		_need_refresh |= ImGui::Button("REFRESH");
		ImGui::Text("Pending Rays to Compute : %d", _ComputeBatch.GetNb());

		bool SampleNbChange = ImGui::SliderInt("SampleNb", (int*)&_pixel_sample_nb, 1, 250);
		_need_refresh |= SampleNbChange;
		_need_refresh |= ImGui::SliderInt("Max Bounce Depth", (int*)&_max_depth, 1, 50);
		_need_refresh |= ImGui::SliderInt("ComputesPerFrame", (int*)&_compute_per_frames, 1, 10000);

		_need_refresh |= ImGui::ColorPicker4("Background Gradient Top", _background_gradient_top.scalar);
		_need_refresh |= ImGui::ColorPicker4("Background Gradient Bottom", _background_gradient_bottom.scalar);

		if (SampleNbChange && _need_refresh)
			_ComputeHeap = MultipleSharedMemory<ray_compute>(_FullScreenScissors.extent.width * _FullScreenScissors.extent.height + _FullScreenScissors.extent.width * _FullScreenScissors.extent.height * _pixel_sample_nb);
	}
	
	_is_moving = AppContext.in_camera_mode;

	//if refresh is needed or requested, we create a new image
	if (_need_refresh || _is_moving)
		DispatchSceneRay(AppContext);
	else//otherwise we try to converge the current image
	{
		_batch_fence.lock();
		for (uint32_t i = 0; i < AppContext.threadPool.GetThreadsNb(); i++)
		{
			//asking for a Batch for the job to work on with the size user chose
			uint32_t computeNb			= _compute_per_frames;
			const RayBatch& computes	= _ComputeBatch.PopBatch(computeNb);

			if (computeNb > 0)
			{
				AnyHitRaytraceJob newJob(computes, *this);

				//adding the job and asking for immediate execution
				AppContext.threadPool.Add(newJob);
			}
			else
				break;
		}
		_batch_fence.unlock();
	}

	//if we suddenly stop moving, we can start converging, but for that we need a final refresh
	_need_refresh = _is_moving;
}

/*==== Show =====*/

void RaytraceCPU::Show(GAPIHandle& GAPIHandle)
{
	//to record errors
	VkResult result;

	//current frames resources
	VkCommandBuffer commandBuffer	= GAPIHandle.GetCurrentVulkanCommand();
	VkSemaphore waitSemaphore		= GAPIHandle.GetCurrentCanPresentSemaphore();
	VkSemaphore signalSemaphore		= GAPIHandle.GetCurrentHasPresentedSemaphore();

	//begin command buffer
	{
		//first, reset previous records
		VK_CALL_PRINT(vkResetCommandBuffer(commandBuffer, 0));

		//then open for record
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VK_CALL_PRINT(vkBeginCommandBuffer(commandBuffer, &info));

	}
	//copy CPU Buffer into GPU Buffer
	{
		void* CPUMap;
		VK_CALL_PRINT(vkMapMemory(GAPIHandle._VulkanDevice, _ImageCopyMemory, _ImageCopyBufferSize * GAPIHandle._vk_current_frame, _ImageCopyBufferSize, 0, &CPUMap));
		memcpy(CPUMap, *_RaytracedImage, _ImageCopyBufferSize);
		vkUnmapMemory(GAPIHandle._VulkanDevice, _ImageCopyMemory);
	}

	// copying from staging buffer to image
	{
		// changing the image to allow transfer from buffer
		VkImageMemoryBarrier barrier{};
		barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout						= VK_IMAGE_LAYOUT_UNDEFINED;//from nothing
		barrier.newLayout						= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;//to transfer dest
		barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.image							= _GPULocalImageBuffers[GAPIHandle._vk_current_frame];
		barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel	= 0;
		barrier.subresourceRange.levelCount		= 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount		= 1;
		barrier.srcAccessMask					= 0;// making the image accessible to
		barrier.dstAccessMask					= VK_ACCESS_TRANSFER_WRITE_BIT;// ... resources during TRANSFER WRITE

		//layout should change when we go from pipeline doing nothing to transfer stage
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		//copy the full image
		VkBufferImageCopy copyRegion{};
		copyRegion.imageSubresource.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.layerCount	= 1;
		copyRegion.imageExtent.width			= _FullScreenScissors.extent.width;
		copyRegion.imageExtent.height			= _FullScreenScissors.extent.height;
		copyRegion.imageExtent.depth			= 1;

		vkCmdCopyBufferToImage(commandBuffer, _ImageCopyBuffer[GAPIHandle._vk_current_frame], _GPULocalImageBuffers[GAPIHandle._vk_current_frame], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// changing the image to allow read from shader
		barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout						= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;//from nothing
		barrier.newLayout						= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//to transfer dest
		barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.image							= _GPULocalImageBuffers[GAPIHandle._vk_current_frame];
		barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel	= 0;
		barrier.subresourceRange.levelCount		= 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount		= 1;
		barrier.srcAccessMask					= VK_ACCESS_TRANSFER_WRITE_BIT;// making the image accessible for ...
		barrier.dstAccessMask					= VK_ACCESS_SHADER_READ_BIT; // shaders

		//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	//begin render pass onto whole screen
	{
		//Set output and output settings for this render pass.
		VkRenderPassBeginInfo info = {};
		info.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass			= _FullScreenRenderPass;
		info.framebuffer		= _FullScreenOutput[GAPIHandle._vk_frame_index];
		info.renderArea			= _FullScreenScissors;
		info.clearValueCount	= 1;

		//clear our output (grey for backbuffer)
		VkClearValue clearValue{};
		clearValue.color = { 0.2f, 0.2f, 0.2f, 1.0f };
		info.pClearValues = &clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//bind pipeline, descriptor, viewport and scissors ...
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _FullScreenPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _FullScreenLayout, 0, 1, &_GPUImageDescriptorSets[GAPIHandle._vk_current_frame], 0, nullptr);
	vkCmdSetViewport(commandBuffer, 0, 1, &_FullScreenViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_FullScreenScissors);

	// ... then draw !
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

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
		result = vkEndCommandBuffer(commandBuffer);
		//... and submit it straight away
		result = vkQueueSubmit(GAPIHandle._VulkanQueues[0], 1, &info, nullptr);
	}
}

/*==== Close =====*/

void RaytraceCPU::Close(GraphicsAPIManager& GAPI)
{
	//clear the fullscreen images buffer
	VK_CLEAR_ARRAY(_ImageCopyBuffer, GAPI._nb_vk_frames, vkDestroyBuffer, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_GPULocalImageViews, GAPI._nb_vk_frames, vkDestroyImageView, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_GPULocalImageBuffers, GAPI._nb_vk_frames, vkDestroyImage, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_FullScreenOutput, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
	_MappedCPUImage.Clear();
	_RaytracedImage.Clear();

	//free the allocated memory for the fullscreen images
	vkFreeMemory(GAPI._VulkanDevice, _GPULocalImageMemory, nullptr);
	vkFreeMemory(GAPI._VulkanDevice, _ImageCopyMemory, nullptr);

	//destroy the descriptors associated with the images
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _GPUImageDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _GPUImageDescriptorLayout, nullptr);
	vkDestroySampler(GAPI._VulkanDevice, _FullScreenSampler, nullptr);

	//destroy piupeline and associated objects
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _FullScreenLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _FullScreenPipeline, nullptr);
	vkDestroyRenderPass(GAPI._VulkanDevice, _FullScreenRenderPass, nullptr);
}
