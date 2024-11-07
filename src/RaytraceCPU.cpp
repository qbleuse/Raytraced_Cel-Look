#include "RaytraceCPU.h"

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


	CreateVulkanShaders(GAPI.VulkanUploader, VertexShader, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader, "Raytrace Fullscreen Vertex");
	CreateVulkanShaders(GAPI.VulkanUploader, FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader, "Raytrace Fullscreen Frag");
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
	viewportStateInfo.pViewports	= &fullscreenViewport;
	viewportStateInfo.scissorCount	= 1;
	viewportStateInfo.pScissors		= &fullscreenScissors;


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
		frameColourBufferAttachment.format			= GAPI.VulkanSurfaceFormat.format;
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

		VK_CALL_PRINT(vkCreateRenderPass(GAPI.VulkanDevice, &renderPassInfo, nullptr, &fullscreenRenderPass));
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		// a very basic sampler will be fine
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		vkCreateSampler(GAPI.VulkanDevice, &samplerInfo, nullptr, &fullscreenSampler);


		//the sampler and image to sample the CPU texture
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding			= 0;
		samplerLayoutBinding.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount	= 1;
		samplerLayoutBinding.stageFlags			= VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = &fullscreenSampler;


		//the descriptor layout that include the immutable sampler and the local GPU image
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings	= &samplerLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI.VulkanDevice, &layoutInfo, nullptr, &GPUImageDescriptorLayout));

	}

	{
		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts		= &GPUImageDescriptorLayout;
		pipelineLayoutInfo.setLayoutCount	= 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI.VulkanDevice, &pipelineLayoutInfo, nullptr, &fullscreenLayout));
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
	pipelineInfo.layout					= fullscreenLayout;
	pipelineInfo.renderPass				= fullscreenRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI.VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &fullscreenPipeline));

	vkDestroyShaderModule(GAPI.VulkanDevice, FragmentShader, nullptr);
	vkDestroyShaderModule(GAPI.VulkanDevice, VertexShader, nullptr);
}



void RaytraceCPU::Prepare(GraphicsAPIManager& GAPI)
{
	VkShaderModule VertexShader, FragmentShader;

	PrepareVulkanScripts(GAPI, VertexShader, FragmentShader);
	PrepareVulkanProps(GAPI, VertexShader, FragmentShader);

	scene.Alloc(2);
	scene[0] = new sphere(vec3{ 0.0f,0.0f,-1.0f }, 0.5f);
	scene[1] = new sphere(vec3{ 0.0f,-100.5f,-1.0f }, 100.0f);
}


/*==== Resize =====*/


void RaytraceCPU::ResizeVulkanResource(GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	//clear the fullscreen images buffer
	VK_CLEAR_ARRAY(ImageCopyBuffer, old_nb_frames, vkDestroyBuffer, GAPI.VulkanDevice);
	VK_CLEAR_ARRAY(GPULocalImageViews, old_nb_frames, vkDestroyImageView, GAPI.VulkanDevice);
	VK_CLEAR_ARRAY(GPULocalImageBuffers, old_nb_frames, vkDestroyImage, GAPI.VulkanDevice);
	VK_CLEAR_ARRAY(fullscreenOutput, GAPI.NbVulkanFrames, vkDestroyFramebuffer, GAPI.VulkanDevice);
	MappedCPUImage.Clear();
	raytracedImage.Clear();

	//free the allocated memory for the fullscreen images
	vkFreeMemory(GAPI.VulkanDevice, GPULocalImageMemory, nullptr);
	vkFreeMemory(GAPI.VulkanDevice, ImageCopyMemory, nullptr);
	vkDestroyDescriptorPool(GAPI.VulkanDevice, GPUImageDescriptorPool, nullptr);

	//filling up the viewport and scissors
	fullscreenViewport.width	= (float)GAPI.VulkanWidth;
	fullscreenViewport.height	= (float)GAPI.VulkanHeight;
	fullscreenViewport.maxDepth = 1.0f;
	fullscreenViewport.minDepth = 0.0f;
	fullscreenViewport.x		= 0.0f;
	fullscreenViewport.y		= 0.0f;
	fullscreenScissors.extent = { (uint32_t)GAPI.VulkanWidth, (uint32_t)GAPI.VulkanHeight };
	raytracedImage.Alloc(GAPI.VulkanWidth * GAPI.VulkanHeight);

	//reallocate the fullscreen image buffers with the new number of available images
	fullscreenOutput.Alloc(GAPI.NbVulkanFrames);
	ImageCopyBuffer.Alloc(GAPI.NbVulkanFrames);
	GPULocalImageViews.Alloc(GAPI.NbVulkanFrames);
	GPULocalImageBuffers.Alloc(GAPI.NbVulkanFrames);
	MappedCPUImage.Alloc(GAPI.NbVulkanFrames);

	//recreate the descriptor pool
	{
		//there ios only sampler and image in the descriptor pool
		VkDescriptorPoolSize poolUniformSize{};
		poolUniformSize.type			= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolUniformSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 1;
		poolInfo.pPoolSizes		= &poolUniformSize;
		poolInfo.maxSets		= GAPI.NbVulkanFrames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI.VulkanDevice, &poolInfo, nullptr, &GPUImageDescriptorPool))
	}

	//allocate the memory for descriptor sets
	{
		//describe a descriptor set for each of our framebuffer (a uniform bufer per frame)
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= GPUImageDescriptorPool;
		allocInfo.descriptorSetCount	= GAPI.NbVulkanFrames;

		//make a copy of our loayout for the number of frames needed
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI.NbVulkanFrames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
			memcpy(&layouts[i], &GPUImageDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//Allocate descriptor sets on CPU and GPU side
		GPUImageDescriptorSets.Alloc(GAPI.NbVulkanFrames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI.VulkanDevice, &allocInfo, *GPUImageDescriptorSets));
	}

	// the size of the fullscreen image's buffer : a fullscreen rgba texture
	VkDeviceSize bufferSize = static_cast<VkDeviceSize>(fullscreenViewport.width * fullscreenViewport.height * sizeof(vec4));


	//allocate the memory for buffer and image
	{
		//allocate memory for buffer
		{
			VulkanHelper::CreateVulkanBuffer(GAPI.VulkanUploader, bufferSize * static_cast<VkDeviceSize>(GAPI.NbVulkanFrames), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ImageCopyBuffer[0], ImageCopyMemory, 0, true);
			
			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(GAPI.VulkanDevice, ImageCopyBuffer[0], &memRequirements);
			ImageCopyBufferSize = memRequirements.size / static_cast<VkDeviceSize>(GAPI.NbVulkanFrames);

			vkDestroyBuffer(GAPI.VulkanDevice, ImageCopyBuffer[0], nullptr);
		}

		// creating the image object with data given in parameters
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType		= VK_IMAGE_TYPE_2D;//we copy a fullscreen image
		imageInfo.format		= VK_FORMAT_R32G32B32A32_SFLOAT;//easier to manipulate (may be unsuppoorted by some GPUs)
		imageInfo.extent.width	= fullscreenScissors.extent.width;
		imageInfo.extent.height = fullscreenScissors.extent.height;
		imageInfo.extent.depth	= 1;
		imageInfo.mipLevels		= 1u;
		imageInfo.arrayLayers	= 1u;
		imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL;//will only load images from buffers, so no need to change this
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//same this can be changed later and each scene will do what they need on their own
		imageInfo.usage			= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;//this is slighlty more important as we may use image as deffered rendering buffers, so made it available
		imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;//we have to command queues : one for the app, one for the UI. the UI shouldn't use iamge we create.
		imageInfo.samples		= VK_SAMPLE_COUNT_1_BIT;//why would you want more than one sample in a simple app ?
		imageInfo.flags = 0; // Optional

		VK_CALL_PRINT(vkCreateImage(GAPI.VulkanDevice, &imageInfo, nullptr, &GPULocalImageBuffers[0]));

		//getting the necessary requirements to create our image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(GAPI.VulkanDevice, GPULocalImageBuffers[0], &memRequirements);
		GPULocalImageBufferSize = memRequirements.size;

		//trying to find a matching memory type between what the app wants and the device's limitation.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size * GAPI.NbVulkanFrames;//we want multiple frames
		allocInfo.memoryTypeIndex = VulkanHelper::GetMemoryTypeFromRequirements(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements,GAPI.VulkanUploader.MemoryProperties);

		//allocating and associate the memory to our image.
		VK_CALL_PRINT(vkAllocateMemory(GAPI.VulkanDevice, &allocInfo, nullptr, &GPULocalImageMemory));

		vkDestroyImage(GAPI.VulkanDevice, GPULocalImageBuffers[0], nullptr);
	}

	//describing the framebuffer to the swapchain
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType		= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass	= fullscreenRenderPass;
	framebufferInfo.width		= GAPI.VulkanWidth;
	framebufferInfo.height		= GAPI.VulkanHeight;
	framebufferInfo.layers		= 1;

	for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
	{
		//make our framebuffer linked to the swapchain back buffers
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &GAPI.VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI.VulkanDevice, &framebufferInfo, nullptr, &fullscreenOutput[i]))

		//create the buffer object that will be used to copy from CPU to GPU
		VulkanHelper::CreateVulkanBuffer(GAPI.VulkanUploader, ImageCopyBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ImageCopyBuffer[i], ImageCopyMemory, ImageCopyBufferSize * i, false);

		//create the image that will be used to write from texture to screen frame buffer
		VulkanHelper::CreateImage(GAPI.VulkanUploader, GPULocalImageBuffers[i], GPULocalImageMemory, fullscreenScissors.extent.width, fullscreenScissors.extent.height, 1, VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, GPULocalImageBufferSize * i, false);

		// changing the image to allow read from shader
		VkImageMemoryBarrier barrier{};
		barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout						= VK_IMAGE_LAYOUT_UNDEFINED;//from nothing
		barrier.newLayout						= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//to transfer dest
		barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.image							= GPULocalImageBuffers[i];
		barrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel	= 0;
		barrier.subresourceRange.levelCount		= 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount		= 1;
		barrier.srcAccessMask					= 0;// making the image accessible for ...
		barrier.dstAccessMask					= VK_ACCESS_SHADER_READ_BIT; // shaders

		//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
		vkCmdPipelineBarrier(GAPI.VulkanUploader.CopyBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		//create an image view associated with the GPU local Image to make it available in shader
		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType						= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.subresourceRange.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.format						= VK_FORMAT_R32G32B32A32_SFLOAT;
		viewCreateInfo.image						= GPULocalImageBuffers[i];
		viewCreateInfo.subresourceRange.layerCount	= 1;
		viewCreateInfo.subresourceRange.levelCount	= 1;
		viewCreateInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;

		VK_CALL_PRINT(vkCreateImageView(GAPI.VulkanDevice, &viewCreateInfo, nullptr, &GPULocalImageViews[i]));

		//wrinting our newly created imatge view in descriptor.
		//sampler is immutable so no need to send it.
		VkDescriptorImageInfo imageViewInfo{};
		imageViewInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageViewInfo.imageView = GPULocalImageViews[i];

		VkWriteDescriptorSet imageDescriptorWrite{};
		imageDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageDescriptorWrite.dstSet				= GPUImageDescriptorSets[i];
		imageDescriptorWrite.dstBinding			= 0;
		imageDescriptorWrite.dstArrayElement	= 0;
		imageDescriptorWrite.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imageDescriptorWrite.descriptorCount	= 1;
		imageDescriptorWrite.pImageInfo			= &imageViewInfo;

		vkUpdateDescriptorSets(GAPI.VulkanDevice, 1, &imageDescriptorWrite, 0, nullptr);
	}
}

void RaytraceCPU::Resize(GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
	needRefresh = true;
}


/*==== Act =====*/

vec4 GetRayColor(const ray& incoming)
{
	float backgroundGradient = 0.5f * (incoming.direction.y + 1.0f);
	return vec4{ 1.0f, 1.0f, 1.0f, 1.0f } * (1.0f - backgroundGradient) + vec4 { 0.3f, 0.7f, 1.0f, 1.0f } * backgroundGradient;
}

void RaytraceCPU::DispatchSceneRay(AppWideContext& AppContext)
{
	rayToCompute.Clear();

	//2D viewport values
	float windowHeight	= static_cast<float>(fullscreenScissors.extent.height);
	float windowWidth	= static_cast<float>(fullscreenScissors.extent.width);
	float aspectRatio	= windowWidth / windowHeight;

	//3D viewport and Camera values
	float focalLength			= 1.0f;
	float viewportHeight		= 2.0f;
	float viewportWidth			= viewportHeight * (windowWidth / windowHeight);
	const vec3& cameraCenter	= AppContext.camera_pos;

	//The ray generation sample vectors
	vec3 viewportU = vec3{ viewportWidth, 0.0f, 0.0f };
	vec3 viewportV = vec3{ 0.0f, -viewportHeight, 0.0f };
	vec3 pixelDeltaU = viewportU / windowWidth;
	vec3 pixelDeltaV = viewportV / windowHeight;

	//first sample dir and sample steps
	vec3 viewportUpperLeft	= cameraCenter - vec3{ 0,0,focalLength } - (viewportU * 0.5f) - (viewportV * 0.5f);
	vec3 firstPixel			= viewportUpperLeft + ((pixelDeltaU + pixelDeltaV) * 0.5f);

	ray_compute* computes = (ray_compute*)malloc(sizeof(ray_compute) * fullscreenScissors.extent.width * fullscreenScissors.extent.height);

	for (uint32_t h = 0; h < fullscreenScissors.extent.height; h++)
		for (uint32_t w = 0; w < fullscreenScissors.extent.width; w++)
		{
			//first get the pixel's "3D position"
			vec3 pixelCenter = firstPixel + pixelDeltaU * (float)(w)+pixelDeltaV * (float)(h);
			//create a direction from the camera's position to the 3D viewport for this pixel
			vec3 rayDir = normalize(pixelCenter - cameraCenter);//normalizing is very slow, we'll still do it, but you may want to ignore it

			ray pixelRay = ray{ pixelCenter, rayDir };

			computes[(h * fullscreenScissors.extent.width) + w] = { pixelRay, &raytracedImage[(h * fullscreenScissors.extent.width) + w] };
		}

	rayToCompute.PushBatch(computes, fullscreenScissors.extent.width * fullscreenScissors.extent.height);
	needRefresh = false;

	for (uint32_t i = 0; i < fullscreenScissors.extent.width * fullscreenScissors.extent.height;)
	{
		uint32_t computeNb = computePerFrames;
		ray_compute* computes_job = rayToCompute.PopBatch(computeNb);

		if (computeNb > 0)
		{
			FirstContactRaytraceJob newJob;
			newJob.ended	= nullptr;
			newJob.computes = computes_job;
			newJob.computesNb = computeNb;
			newJob.hittables = &scene;
			newJob.newRays = &rayToCompute;
			newJob.newRaysFence = &queue_fence;
			newJob.newRays_wait = &queue_wait;

			AppContext.threadPool.Add(newJob);
		}

		i += computeNb;
	}

	//{
	//	uint32_t computeNb = computePerFrames;
	//	ray_compute* computes_job = rayToCompute.PopBatch(computeNb);
	//
	//	if (computeNb > 0)
	//	{
	//		FirstContactRaytraceJob newJob;
	//		newJob.ended = nullptr;
	//		newJob.computes = computes_job;
	//		newJob.computesNb = computeNb;
	//		newJob.hittables = &scene;
	//		newJob.newRays = &rayToCompute;
	//
	//		AppContext.threadPool.Add(newJob);
	//	}
	//}
}

void RaytraceCPU::Act(AppWideContext& AppContext)
{
	if (needRefresh || AppContext.in_camera_mode)
		DispatchSceneRay(AppContext);

	queue_fence.lock();

	//queue_wait.wait(queue_fence);

	for (uint32_t i = 0; i < AppContext.threadPool.GetThreadsNb(); i++)
	{
		uint32_t computeNb = computePerFrames;
		ray_compute* computes = rayToCompute.PopBatch(computeNb);
	
		if (computeNb > 0)
		{
			DiffuseRaytraceJob newJob;
			newJob.ended = nullptr;
			newJob.computes = computes;
			newJob.computesNb = computeNb;
			newJob.hittables = &scene;
			newJob.newRays = &rayToCompute;
			newJob.newRaysFence = &queue_fence;
			newJob.newRays_wait = &queue_wait;
	
			AppContext.threadPool.Add(newJob);
		}
		else
			break;
	}

	queue_fence.unlock();

	//queue_wait.notify_one();
}

/*==== Show =====*/

void RaytraceCPU::Show(GAPIHandle& GAPIHandle)
{
	VkResult result;
	VkCommandBuffer commandBuffer = GAPIHandle.GetCurrentVulkanCommand();
	VkSemaphore		waitSemaphore = GAPIHandle.GetCurrentCanPresentSemaphore();
	VkSemaphore		signalSemaphore = GAPIHandle.GetCurrentHasPresentedSemaphore();

	//begin command buffer
	{
		result = vkResetCommandBuffer(commandBuffer, 0);
		//check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		result = vkBeginCommandBuffer(commandBuffer, &info);
		//	check_vk_result(err);
	}
	//copy CPU Buffer into GPU Buffer
	{
		void* CPUMap;
		VK_CALL_PRINT(vkMapMemory(GAPIHandle.VulkanDevice, ImageCopyMemory, ImageCopyBufferSize * GAPIHandle.VulkanCurrentFrame, ImageCopyBufferSize, 0, &CPUMap));
		memcpy(CPUMap, *raytracedImage, ImageCopyBufferSize);
		vkUnmapMemory(GAPIHandle.VulkanDevice, ImageCopyMemory);
	}

	// copying from staging buffer to image
	{
		// changing the image to allow transfer from buffer
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;//from nothing
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;//to transfer dest
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.image = GPULocalImageBuffers[GAPIHandle.VulkanCurrentFrame];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;// making the image accessible to
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;// ... resources during TRANSFER WRITE

		//layout should change when we go from pipeline doing nothing to transfer stage
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy copyRegion{};
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageExtent.width = fullscreenScissors.extent.width;
		copyRegion.imageExtent.height = fullscreenScissors.extent.height;
		copyRegion.imageExtent.depth = 1;
		

		vkCmdCopyBufferToImage(commandBuffer, ImageCopyBuffer[GAPIHandle.VulkanCurrentFrame], GPULocalImageBuffers[GAPIHandle.VulkanCurrentFrame], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		// changing the image to allow read from shader
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;//from nothing
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//to transfer dest
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.image = GPULocalImageBuffers[GAPIHandle.VulkanCurrentFrame];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;// making the image accessible for ...
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // shaders

		//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	//begin render pass onto whole screen
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass			= fullscreenRenderPass;
		info.framebuffer		= fullscreenOutput[GAPIHandle.VulkanFrameIndex];
		info.renderArea			= fullscreenScissors;
		info.clearValueCount	= 1;
		VkClearValue clearValue{};
		clearValue.color = { 0.2f, 0.2f, 0.2f, 1.0f };
		info.pClearValues = &clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	

	//bind pipeline, descriptor, viewport and scissors ...
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenLayout, 0, 1, &GPUImageDescriptorSets[GAPIHandle.VulkanCurrentFrame], 0, nullptr);
	vkCmdSetViewport(commandBuffer, 0, 1, &fullscreenViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &fullscreenScissors);

	// ... then draw !
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

		result = vkEndCommandBuffer(commandBuffer);
		//check_vk_result(err);
		result = vkQueueSubmit(GAPIHandle.VulkanQueues[0], 1, &info, nullptr);
		//check_vk_result(err);
	}
}

/*==== Close =====*/

void RaytraceCPU::Close(GraphicsAPIManager& GAPI)
{
	//clear the fullscreen images buffer
	VK_CLEAR_ARRAY(ImageCopyBuffer, GAPI.NbVulkanFrames, vkDestroyBuffer, GAPI.VulkanDevice);
	VK_CLEAR_ARRAY(GPULocalImageViews, GAPI.NbVulkanFrames, vkDestroyImageView, GAPI.VulkanDevice);
	VK_CLEAR_ARRAY(GPULocalImageBuffers, GAPI.NbVulkanFrames, vkDestroyImage, GAPI.VulkanDevice);
	VK_CLEAR_ARRAY(fullscreenOutput, GAPI.NbVulkanFrames, vkDestroyFramebuffer, GAPI.VulkanDevice);
	MappedCPUImage.Clear();
	raytracedImage.Clear();

	//free the allocated memory for the fullscreen images
	vkFreeMemory(GAPI.VulkanDevice, GPULocalImageMemory, nullptr);
	vkFreeMemory(GAPI.VulkanDevice, ImageCopyMemory, nullptr);

	//destroy the descriptors associated with the images
	vkDestroyDescriptorPool(GAPI.VulkanDevice, GPUImageDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(GAPI.VulkanDevice, GPUImageDescriptorLayout, nullptr);
	vkDestroySampler(GAPI.VulkanDevice, fullscreenSampler, nullptr);

	//destroy piupeline and associated objects
	vkDestroyPipelineLayout(GAPI.VulkanDevice, fullscreenLayout, nullptr);
	vkDestroyPipeline(GAPI.VulkanDevice, fullscreenPipeline, nullptr);
	vkDestroyRenderPass(GAPI.VulkanDevice, fullscreenRenderPass, nullptr);
}