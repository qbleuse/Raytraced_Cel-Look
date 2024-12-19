#include "RaytraceGPU.h"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"

/*==== Prepare =====*/

void RaytraceGPU::PrepareVulkanRaytracingProps(GraphicsAPIManager& GAPI, VkShaderModule& RayGenShader, VkShaderModule& MissShader, VkShaderModule& HitShader)
{
	VkResult result = VK_SUCCESS;

	/*===== MODEL LOADING & AS BUILDING ======*/

	//load the model
	VulkanHelper::LoadObjFile(GAPI._VulkanUploader, "../../../media/teapot/teapot.obj", _RayModel._Meshes);

	//create bottom level AS from model
	VulkanHelper::CreateRaytracedGeometryFromMesh(GAPI._VulkanUploader, _RayBottomAS, _RayModel._Meshes);

	//create Top level AS from geometry with identity matrix
	VulkanHelper::UploadRaytracedModelFromGeometry(GAPI._VulkanUploader, _RayTopAS, identity(), _RayBottomAS);

	/*===== DESCRIBE SHADER STAGE AND GROUPS =====*/

	//the shader stages we'll give to the pipeline
	MultipleVolatileMemory<VkPipelineShaderStageCreateInfo> shaderStages{ static_cast<VkPipelineShaderStageCreateInfo*>(alloca(sizeof(VkPipelineShaderStageCreateInfo) * 3)) };
	memset(*shaderStages, 0, sizeof(VkPipelineShaderStageCreateInfo) * 3);
	//the shader groups, to allow the pipeline to create the shader table
	MultipleVolatileMemory<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{ static_cast<VkRayTracingShaderGroupCreateInfoKHR*>(alloca(sizeof(VkRayTracingShaderGroupCreateInfoKHR) * 3)) };
	memset(*shaderGroups, 0, sizeof(VkRayTracingShaderGroupCreateInfoKHR) * 3);

	//first, we'll describe ray gen
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shaderStages[0].module = RayGenShader;
	shaderStages[0].pName = "main";

	//then, miss shader
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shaderStages[1].module = MissShader;
	shaderStages[1].pName = "main";

	//finally, closest hit shader
	shaderStages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shaderStages[2].module = HitShader;
	shaderStages[2].pName = "main";

	//describing our raygen group, it only includes the first shader of our pipeline
	shaderGroups[0].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[0].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[0].generalShader		= 0;
	shaderGroups[0].closestHitShader	= VK_SHADER_UNUSED_KHR;
	shaderGroups[0].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[0].intersectionShader	= VK_SHADER_UNUSED_KHR;

	//describing our miss group, it only includes the second shader of our pipeline, as we have not described multiple miss
	shaderGroups[1].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[1].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shaderGroups[1].generalShader		= 1;
	shaderGroups[1].closestHitShader	= VK_SHADER_UNUSED_KHR;
	shaderGroups[1].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[1].intersectionShader	= VK_SHADER_UNUSED_KHR;

	//describing our hit group, it only includes the third shader of our pipeline, as we have not described multiple hits
	shaderGroups[2].sType				= VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroups[2].type				= VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	shaderGroups[2].generalShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[2].closestHitShader	= 2;
	shaderGroups[2].anyHitShader		= VK_SHADER_UNUSED_KHR;
	shaderGroups[2].intersectionShader	= VK_SHADER_UNUSED_KHR;

	/*===== PIPELINE ATTACHEMENT =====*/

	{
		//creating our acceleration structure binder
		VkDescriptorSetLayoutBinding ASLayoutBinding{};
		ASLayoutBinding.binding			= 0;
		ASLayoutBinding.descriptorType	= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ASLayoutBinding.descriptorCount = 1;
		ASLayoutBinding.stageFlags		= VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		//create a binding for our colour buffers
		VkDescriptorSetLayoutBinding colourBackBufferBinding{};
		colourBackBufferBinding.binding			= 1;
		colourBackBufferBinding.descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		colourBackBufferBinding.descriptorCount	= 1;
		colourBackBufferBinding.stageFlags			= VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		//create a binding for our colour buffers
		VkDescriptorSetLayoutBinding uniformBufferBinding{};
		uniformBufferBinding.binding		= 2;
		uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformBufferBinding.descriptorCount = 1;
		uniformBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 3;
		VkDescriptorSetLayoutBinding layoutsBinding[3] = {ASLayoutBinding , colourBackBufferBinding, uniformBufferBinding};
		layoutInfo.pBindings = layoutsBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_RayDescriptorLayout));
	}

	{
		//our pipeline layout is the same as the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts		= &_RayDescriptorLayout;
		pipelineLayoutInfo.setLayoutCount	= 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_RayLayout));
	}

	/*===== PIPELINE CREATION =====*/

	//our raytracing pipeline
	VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
	pipelineInfo.sType							= VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	pipelineInfo.stageCount						= 3;
	pipelineInfo.pStages						= *shaderStages;
	pipelineInfo.groupCount						= 3;
	pipelineInfo.pGroups						= *shaderGroups;
	pipelineInfo.maxPipelineRayRecursionDepth	= 1;//for the moment we'll hardcode the value, we'll see if we'll make it editable
	pipelineInfo.layout							= _RayLayout;//describe the attachement

	VK_CALL_KHR(GAPI._VulkanDevice, vkCreateRayTracingPipelinesKHR, GAPI._VulkanDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_RayPipeline);


	/*===== SHADER BINDING TABLE CREATION =====*/
	
	//allocating space for the address of the SBT
	_RayShaderBindingAddress.Alloc(3);

	{
		//this machine's specific GPU properties to get the SBT alignement
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
		rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

		//needed to get the above info
		VkPhysicalDeviceProperties2 GPUProperties{};
		GPUProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		GPUProperties.pNext = &rtProperties;
		vkGetPhysicalDeviceProperties2(GAPI._VulkanGPU, &GPUProperties);

		//the needed alignement to create our shader binding table
		uint32_t handleSize			= rtProperties.shaderGroupHandleSize;
		uint32_t handleAlignment	= rtProperties.shaderGroupHandleAlignment;
		uint32_t baseAlignment		= rtProperties.shaderGroupBaseAlignment;
		uint32_t handleSizeAligned	= VulkanHelper::AlignUp(handleSize, handleAlignment);
		uint32_t startSizeAligned	= VulkanHelper::AlignUp(handleSizeAligned, baseAlignment);

		//raygen shader's aligned stride and size
		_RayShaderBindingAddress[0].stride	= startSizeAligned;//there will only be one but still
		_RayShaderBindingAddress[0].size	= startSizeAligned;

		//miss shader's aligned stride and size
		_RayShaderBindingAddress[1].stride	= handleSizeAligned;
		_RayShaderBindingAddress[1].size	= startSizeAligned;

		//hit shader's aligned stride and size
		_RayShaderBindingAddress[2].stride	= handleSizeAligned;
		_RayShaderBindingAddress[2].size	= startSizeAligned;

		//creating a GPU buffer for the Shader Binding Table
		VulkanHelper::CreateVulkanBuffer(GAPI._VulkanUploader, 3 * startSizeAligned, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _RayShaderBindingBuffer, _RayShaderBindingMemory, 0, true, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT);

		//get back handle from shader group created in pipeline
		MultipleVolatileMemory<uint8_t> shaderGroupHandle{ (uint8_t*)alloca(3 * handleSize) };
		VK_CALL_KHR( GAPI._VulkanDevice, vkGetRayTracingShaderGroupHandlesKHR, GAPI._VulkanDevice, _RayPipeline, 0, 3, 3 * handleSize, *shaderGroupHandle);

		//mapping memory to buffer
		uint8_t* CPUSBTBufferMap;
		vkMapMemory(GAPI._VulkanDevice, _RayShaderBindingMemory, 0, 3 * startSizeAligned, 0, (void**)(& CPUSBTBufferMap));

		//memcpy(CPUSBTBufferMap, *shaderGroupHandle, 3 * startSizeAligned);
		//copying every address to the shader table
		memcpy(CPUSBTBufferMap, *shaderGroupHandle, handleSize);
		memcpy(CPUSBTBufferMap + startSizeAligned, *shaderGroupHandle + handleSize, handleSize);
		memcpy(CPUSBTBufferMap + 2 * startSizeAligned, *shaderGroupHandle + 2 * handleSize, handleSize);

		//unmap and copy
		vkUnmapMemory(GAPI._VulkanDevice, _RayShaderBindingMemory);

		//get our GPU Buffer's address
		VkBufferDeviceAddressInfo addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addressInfo.buffer = _RayShaderBindingBuffer;
		VkDeviceAddress GPUAddress = vkGetBufferDeviceAddress(GAPI._VulkanDevice, &addressInfo);

		_RayShaderBindingAddress[0].deviceAddress = GPUAddress;
		_RayShaderBindingAddress[1].deviceAddress = GPUAddress + _RayShaderBindingAddress[0].size;
		_RayShaderBindingAddress[2].deviceAddress = GPUAddress + _RayShaderBindingAddress[0].size + _RayShaderBindingAddress[1].size;
	}
}

void RaytraceGPU::PrepareVulkanRaytracingScripts(class GraphicsAPIManager& GAPI, VkShaderModule& RayGenShader, VkShaderModule& MissShader, VkShaderModule& HitShader)
{
	//define ray gen shader
	const char* ray_gen_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			layout(location = 0) rayPayloadEXT vec3 payload;

			layout(binding = 0) uniform accelerationStructureEXT topLevelAS;
			layout(binding = 1, rgba32f) uniform image2D image;
			layout(binding = 2) uniform Transform
			{
				mat4 model;
				mat4 view;
				mat4 proj;
			};

			void main()
			{
				const vec2 pixelCenter = gl_LaunchIDEXT.xy + vec2(0.5);

				//finding the pixel we are computing from the ray launch arguments
				const vec2 inUV = (pixelCenter) / vec2(gl_LaunchSizeEXT.xy);

				vec2 d = inUV * 2.0 - 1.0;

				//for the moment, fixed
				vec4 origin = -view[3];
				
				//the targets are on a viewport ahead of us by 3
				vec4 target = proj * vec4(d,1.0,1.0);
				target = target/target.w;

				//creating a direction for our ray
				vec4 direction = view * vec4(normalize(target.xyz),0.0);

				payload = vec3(0.0);

				//tracing rays
				traceRayEXT(
					topLevelAS,//Acceleration structure
					gl_RayFlagsOpaqueEXT,//opaque flags (definelty don't want it forever)
					0xff,//cull mask
					0,0,0,//shader binding table offset, stride, and miss index (as this function can be called from all hit shader)
					origin.xyz,//our camera's origin
					0.001,//our collision epsilon
					direction.xyz,//the direction of our ray
					10000.0,//the max length of our ray (this could be changed if we added energy conservation rules, I think, but in our case , it is more or less infinity)
					0
				);
				
				//our recursive call finished, let's write into the image
				imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(payload,0.0));

			})";

	//define miss shader
	const char* miss_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			layout(location = 0) rayPayloadInEXT vec3 payload;

			void main()
			{
				vec3 background_color_bottom = vec3(1.0);
				vec3 background_color_top = vec3(0.3, 0.7, 1.0);

				float backgroundGradient = 0.5 * (gl_WorldRayDirectionEXT.y + 1.0);
				payload = background_color_bottom * (1.0 - backgroundGradient) + background_color_top * backgroundGradient;
			})";

	//define closest hit shader
	const char* closest_hit_shader =
		R"(#version 460
			#extension GL_EXT_ray_tracing : enable

			layout(location = 0) rayPayloadInEXT vec3 payload;
			hitAttributeEXT vec3 attribs;

			void main()
			{
				payload = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

			})";

	CreateVulkanShaders(GAPI._VulkanUploader, RayGenShader, VK_SHADER_STAGE_RAYGEN_BIT_KHR, ray_gen_shader, "Raytrace GPU RayGen");
	CreateVulkanShaders(GAPI._VulkanUploader, MissShader, VK_SHADER_STAGE_MISS_BIT_KHR, miss_shader, "Raytrace GPU Miss");
	CreateVulkanShaders(GAPI._VulkanUploader, HitShader, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closest_hit_shader, "Raytrace GPU Closest");
}


void RaytraceGPU::PrepareVulkanProps(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
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
	inputAssembly.sType					= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology				= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
	viewportStateInfo.pViewports	= &_CopyViewport;
	viewportStateInfo.scissorCount	= 1;
	viewportStateInfo.pScissors		= &_CopyScissors;


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
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VK_CALL_PRINT(vkCreateRenderPass(GAPI._VulkanDevice, &renderPassInfo, nullptr, &_CopyRenderPass));
	}

	/*===== SUBPASS ATTACHEMENT ======*/

	{
		// a very basic sampler will be fine
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		vkCreateSampler(GAPI._VulkanDevice, &samplerInfo, nullptr, &_CopySampler);


		//the sampler and image to sample the CPU texture
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 0;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = &_CopySampler;


		//the descriptor layout that include the immutable sampler and the local GPU image
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings	= &samplerLayoutBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_CopyDescriptorLayout));

	}

	{
		//the pipeline layout from the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts = &_CopyDescriptorLayout;
		pipelineLayoutInfo.setLayoutCount = 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_CopyLayout));
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
	pipelineInfo.layout					= _CopyLayout;
	pipelineInfo.renderPass				= _CopyRenderPass;
	pipelineInfo.subpass				= 0;
	pipelineInfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex		= -1;

	VK_CALL_PRINT(vkCreateGraphicsPipelines(GAPI._VulkanDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_CopyPipeline));

	vkDestroyShaderModule(GAPI._VulkanDevice, FragmentShader, nullptr);
	vkDestroyShaderModule(GAPI._VulkanDevice, VertexShader, nullptr);
}

void RaytraceGPU::PrepareVulkanScripts(GraphicsAPIManager& GAPI, VkShaderModule& VertexShader, VkShaderModule& FragmentShader)
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

void RaytraceGPU::Prepare(class GraphicsAPIManager& GAPI)
{
	//preparing hardware raytracing props
	{
		//the shaders needed
		VkShaderModule RayGenShader, MissShader, HitShader;

		//compile the shaders here...
		PrepareVulkanRaytracingScripts(GAPI, RayGenShader, MissShader, HitShader);
		//then create the pipeline
		PrepareVulkanRaytracingProps(GAPI, RayGenShader, MissShader, HitShader);
	}

	_RayBuffer.model = identity();

	//preparing full screen copy pipeline
	{
		//the shaders needed
		VkShaderModule VertexShader, FragmentShader;

		//compile the shaders here...
		PrepareVulkanScripts(GAPI, VertexShader, FragmentShader);
		//then create the pipeline
		PrepareVulkanProps(GAPI, VertexShader, FragmentShader);
	}
}

/*===== Resize =====*/

void RaytraceGPU::ResizeVulkanRaytracingResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//first, we clear previously used resources
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _RayDescriptorPool, nullptr);
	VulkanHelper::ClearUniformBufferHandle(GAPI._VulkanDevice, _RayUniformBuffer);

	/*===== DESCRIPTORS ======*/

	{
		//describing how many descriptor at a time should be allocated
		VkDescriptorPoolSize ASPoolSize{};
		ASPoolSize.type			= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ASPoolSize.descriptorCount = 1;

		VkDescriptorPoolSize imagePoolSize{};
		imagePoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imagePoolSize.descriptorCount = 1;

		VkDescriptorPoolSize bufferPoolSize{};
		bufferPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bufferPoolSize.descriptorCount = 1;

		//creating our descriptor pool to allocate sets for each frame
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 3;
		VkDescriptorPoolSize poolSizes[3] = {ASPoolSize, imagePoolSize, bufferPoolSize};
		poolInfo.pPoolSizes		= poolSizes;
		poolInfo.maxSets		= GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_RayDescriptorPool))

		//allocating a descriptor set for each of our framebuffer (a uniform buffer per frame)
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= _RayDescriptorPool;
		allocInfo.descriptorSetCount	= GAPI._nb_vk_frames;

		//copying the same layout for every descriptor set
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_RayDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//then create the resource
		_RayDescriptorSet.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_RayDescriptorSet));
	}

	/*===== UNIFORM BUFFERS ======*/

	//recreate the uniform buffer
	CreateUniformBufferHandle(GAPI._VulkanUploader, _RayUniformBuffer, GAPI._nb_vk_frames, sizeof(mat4) * 3);

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{

		//Adding our Top Level AS in each descriptor set
		VkWriteDescriptorSetAccelerationStructureKHR ASInfo{};
		ASInfo.sType						= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		ASInfo.pAccelerationStructures		= &_RayTopAS._AccelerationStructure;
		ASInfo.accelerationStructureCount	= 1;
		
		VkWriteDescriptorSet ASDescriptorWrite{};
		ASDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ASDescriptorWrite.dstSet			= _RayDescriptorSet[i];
		ASDescriptorWrite.dstBinding		= 0;
		ASDescriptorWrite.dstArrayElement	= 0;
		ASDescriptorWrite.descriptorType	= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ASDescriptorWrite.descriptorCount	= 1;
		ASDescriptorWrite.pNext				= &ASInfo;

		//bind the back buffer to each descriptor
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageView = _RayWriteImageView[i];
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet imageDescriptorWrite{};
		imageDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageDescriptorWrite.dstSet				= _RayDescriptorSet[i];
		imageDescriptorWrite.dstBinding			= 1;
		imageDescriptorWrite.dstArrayElement	= 0;
		imageDescriptorWrite.descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imageDescriptorWrite.descriptorCount	= 1;
		imageDescriptorWrite.pImageInfo			= &imageInfo;

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = _RayUniformBuffer._GPUBuffer[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(mat4) * 3;

		//then link it to the descriptor
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet			= _RayDescriptorSet[i];
		descriptorWrite.dstBinding		= 2;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo		= &bufferInfo;

		VkWriteDescriptorSet descriptorWrites[3] = { ASDescriptorWrite,  imageDescriptorWrite, descriptorWrite };
		vkUpdateDescriptorSets(GAPI._VulkanDevice, 3, descriptorWrites, 0, nullptr);
	}
}

void RaytraceGPU::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{

	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//clear the fullscreen images buffer
	VK_CLEAR_ARRAY(_RayWriteImageView, old_nb_frames, vkDestroyImageView, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_RayWriteImage, old_nb_frames, vkDestroyImage, GAPI._VulkanDevice);
	VK_CLEAR_ARRAY(_CopyOutput, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);

	

	//free the allocated memory for the fullscreen images
	vkFreeMemory(GAPI._VulkanDevice, _RayImageMemory, nullptr);
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _CopyDescriptorPool, nullptr);


	/*===== VIEWPORT AND SCISSORS ======*/

	//filling up the viewport and scissors
	_CopyViewport.width = (float)GAPI._vk_width;
	_CopyViewport.height = (float)GAPI._vk_height;
	_CopyViewport.maxDepth = 1.0f;
	_CopyViewport.minDepth = 0.0f;
	_CopyViewport.x = 0.0f;
	_CopyViewport.y = 0.0f;
	_CopyScissors.extent = { (uint32_t)GAPI._vk_width, (uint32_t)GAPI._vk_height };

	/*===== CPU SIDE IMAGE BUFFERS ======*/

	//reallocate the fullscreen image buffers with the new number of available images
	_CopyOutput.Alloc(GAPI._nb_vk_frames);
	_RayWriteImageView.Alloc(GAPI._nb_vk_frames);
	_RayWriteImage.Alloc(GAPI._nb_vk_frames);


	/*===== DESCRIPTORS ======*/

	//recreate the descriptor pool
	{
		//there is only sampler and image in the descriptor pool
		VkDescriptorPoolSize poolUniformSize{};
		poolUniformSize.type			= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolUniformSize.descriptorCount = GAPI._nb_vk_frames;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolUniformSize;
		poolInfo.maxSets = GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_CopyDescriptorPool))
	}

	//allocate the memory for descriptor sets
	{
		//describe a descriptor set for each of our framebuffer (a uniform bufer per frame)
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _CopyDescriptorPool;
		allocInfo.descriptorSetCount = GAPI._nb_vk_frames;

		//make a copy of our loayout for the number of frames needed
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_CopyDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//Allocate descriptor sets on CPU and GPU side
		_CopyDescriptorSet.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_CopyDescriptorSet));
	}

	/*===== GPU SIDE IMAGE BUFFERS ======*/

	// the size of the fullscreen image's buffer : a fullscreen rgba texture
	VkDeviceSize imageOffset = 0;

	//allocate the memory for buffer and image
	{

		// creating the image object with data given in parameters
		VkImageCreateInfo imageInfo{};
		imageInfo.sType			= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType		= VK_IMAGE_TYPE_2D;//we copy a fullscreen image
		imageInfo.format		= VK_FORMAT_R32G32B32A32_SFLOAT;//easier to manipulate (may be unsuppoorted by some GPUs)
		imageInfo.extent.width	= _CopyScissors.extent.width;
		imageInfo.extent.height = _CopyScissors.extent.height;
		imageInfo.extent.depth	= 1;
		imageInfo.mipLevels		= 1u;
		imageInfo.arrayLayers	= 1u;
		imageInfo.tiling		= VK_IMAGE_TILING_OPTIMAL;//will only load images from buffers, so no need to change this
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//same this can be changed later and each _Scene will do what they need on their own
		imageInfo.usage			= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;//this is slighlty more important as we may use image as deffered rendering buffers, so made it available
		imageInfo.sharingMode	= VK_SHARING_MODE_EXCLUSIVE;//we have to command queues : one for the app, one for the UI. the UI shouldn't use iamge we create.
		imageInfo.samples		= VK_SAMPLE_COUNT_1_BIT;//why would you want more than one sample in a simple app ?
		imageInfo.flags			= 0; // Optional

		VK_CALL_PRINT(vkCreateImage(GAPI._VulkanDevice, &imageInfo, nullptr, &_RayWriteImage[0]));

		//getting the necessary requirements to create our image
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(GAPI._VulkanDevice, _RayWriteImage[0], &memRequirements);
		imageOffset = memRequirements.size;

		//trying to find a matching memory type between what the app wants and the device's limitation.
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize	= memRequirements.size * GAPI._nb_vk_frames;//we want multiple frames
		allocInfo.memoryTypeIndex	= VulkanHelper::GetMemoryTypeFromRequirements(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memRequirements, GAPI._VulkanUploader._MemoryProperties);

		//allocating and associate the memory to our image.
		VK_CALL_PRINT(vkAllocateMemory(GAPI._VulkanDevice, &allocInfo, nullptr, &_RayImageMemory));

		vkDestroyImage(GAPI._VulkanDevice, _RayWriteImage[0], nullptr);
	}

	/*===== FRAMEBUFFERS ======*/

	//describing the framebuffer to the swapchain
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType		= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass	= _CopyRenderPass;
	framebufferInfo.width		= GAPI._vk_width;
	framebufferInfo.height		= GAPI._vk_height;
	framebufferInfo.layers		= 1;

	/*===== CREATING & LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//make our framebuffer linked to the swapchain back buffers
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &GAPI._VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI._VulkanDevice, &framebufferInfo, nullptr, &_CopyOutput[i]))

		//create the image that will be used to write from texture to screen frame buffer
		VulkanHelper::CreateImage(GAPI._VulkanUploader, _RayWriteImage[i], _RayImageMemory, _CopyScissors.extent.width, _CopyScissors.extent.height, 1, 
			VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imageOffset * i, false);

		// changing the image to allow read from shader
		VkImageMemoryBarrier barrier{};
		barrier.sType						= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout					= VK_IMAGE_LAYOUT_UNDEFINED;//from nothing
		barrier.newLayout					= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//to shader read dest (will be change to storage afterwards)
		barrier.srcQueueFamilyIndex			= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.dstQueueFamilyIndex			= VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
		barrier.image						= _RayWriteImage[i];
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;// making the image accessible for ...
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // shaders

		//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
		vkCmdPipelineBarrier(GAPI._VulkanUploader._CopyBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		//create an image view associated with the GPU local Image to make it available in shader
		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType						= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.subresourceRange.aspectMask	= VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.format						= VK_FORMAT_R32G32B32A32_SFLOAT;
		viewCreateInfo.image						= _RayWriteImage[i];
		viewCreateInfo.subresourceRange.layerCount	= 1;
		viewCreateInfo.subresourceRange.levelCount	= 1;
		viewCreateInfo.viewType						= VK_IMAGE_VIEW_TYPE_2D;

		VK_CALL_PRINT(vkCreateImageView(GAPI._VulkanDevice, &viewCreateInfo, nullptr, &_RayWriteImageView[i]));

		//wrinting our newly created imatge view in descriptor.
		//sampler is immutable so no need to send it.
		VkDescriptorImageInfo imageViewInfo{};
		imageViewInfo.imageLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageViewInfo.imageView		= _RayWriteImageView[i];

		VkWriteDescriptorSet imageDescriptorWrite{};
		imageDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageDescriptorWrite.dstSet				= _CopyDescriptorSet[i];
		imageDescriptorWrite.dstBinding			= 0;
		imageDescriptorWrite.dstArrayElement	= 0;
		imageDescriptorWrite.descriptorType		= VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imageDescriptorWrite.descriptorCount	= 1;
		imageDescriptorWrite.pImageInfo			= &imageViewInfo;

		vkUpdateDescriptorSets(GAPI._VulkanDevice, 1, &imageDescriptorWrite, 0, nullptr);
	}
}



void RaytraceGPU::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
	ResizeVulkanRaytracingResource(GAPI, old_width, old_height, old_nb_frames);

}

/*===== Act =====*/

void RaytraceGPU::Act(struct AppWideContext& AppContext)
{

	//_RayObjData.euler_angles.y += AppContext.delta_time;
	//
	//it will change every frame
	{
		_RayBuffer.proj = inv_perspective_proj(_CopyScissors.extent.width, _CopyScissors.extent.height, AppContext.fov, AppContext.near_plane, AppContext.far_plane);
		_RayBuffer.view = transpose(AppContext.view_mat);
		_RayBuffer.view .x.w = 0.0f;
		_RayBuffer.view .y.w = 0.0f;
		_RayBuffer.view .z.w = 0.0f;
		_RayBuffer.view .w.xyz = AppContext.camera_pos;
		//_RayMatBuffer.model = scale(_RayObjData.scale.x, _RayObjData.scale.y, _RayObjData.scale.z) * ro_intrinsic_rot(_RayObjData.euler_angles.x, _RayObjData.euler_angles.y, _RayObjData.euler_angles.z) * ro_translate(_RayObjData.pos);
	}
	//
	////UI update
	//if (SceneCanShowUI(AppContext))
	//{
	//	ImGui::SliderFloat3("Object Postion", _RayObjData.pos.scalar, -100.0f, 100.0f);
	//	ImGui::SliderFloat3("Object Rotation", _RayObjData.euler_angles.scalar, -180.0f, 180.0f);
	//	ImGui::SliderFloat3("Object Scale", _RayObjData.scale.scalar, 0.0f, 1.0f, "%0.01f");
	//}

}

/*===== Show =====*/

void RaytraceGPU::Show(GAPIHandle& GAPIHandle)
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
	
	// changing the back buffer to be able to being written by pipeline
	VkImageMemoryBarrier barrier{};
	barrier.sType				= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout			= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//from nothing
	barrier.newLayout			= VK_IMAGE_LAYOUT_GENERAL;//to transfer dest
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.image				= _RayWriteImage[GAPIHandle._vk_current_frame];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_NONE;// making the image accessible for ...
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; // shaders

	//the buffer will change every frame as the object rotates every frame
	memcpy(_RayUniformBuffer._CPUMemoryHandle[GAPIHandle._vk_current_frame], (void*)&_RayBuffer, sizeof(mat4) * 3);

	//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayPipeline);
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayLayout, 0, 1, &_RayDescriptorSet[GAPIHandle._vk_current_frame], 0, nullptr);


	VkStridedDeviceAddressRegionKHR tmp{};
	VK_CALL_KHR(GAPIHandle._VulkanDevice, vkCmdTraceRaysKHR, commandBuffer, &_RayShaderBindingAddress[0], &_RayShaderBindingAddress[1], &_RayShaderBindingAddress[2], &tmp, _CopyScissors.extent.width, _CopyScissors.extent.height, 1);

	barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;//from nothing
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;//to transfer dest
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.image = _RayWriteImage[GAPIHandle._vk_current_frame];
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_NONE;// making the image accessible for ...
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; // shaders

	//allowing copy from write iamge to framebuffer
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	//begin render pass onto whole screen
	{
		//Set output and output settings for this render pass.
		VkRenderPassBeginInfo info = {};
		info.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass		= _CopyRenderPass;
		info.framebuffer	= _CopyOutput[GAPIHandle._vk_frame_index];
		info.renderArea		= _CopyScissors;
		info.clearValueCount = 1;

		//clear our output (grey for backbuffer)
		VkClearValue clearValue{};
		clearValue.color = { 0.2f, 0.2f, 0.2f, 1.0f };
		info.pClearValues = &clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//bind pipeline, descriptor, viewport and scissors ...
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _CopyPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _CopyLayout, 0, 1, &_CopyDescriptorSet[GAPIHandle._vk_current_frame], 0, nullptr);
	vkCmdSetViewport(commandBuffer, 0, 1, &_CopyViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_CopyScissors);

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

void RaytraceGPU::Close(class GraphicsAPIManager& GAPI)
{
	//the memory allocated by the Vulkan Helper is volatile : it must be explecitly freed !
	ClearModel(GAPI._VulkanDevice, _RayModel);

	//release descriptors
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _RayDescriptorPool, nullptr);
	_RayDescriptorSet.Clear();
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _RayDescriptorLayout, nullptr);


	//release pipeline objects
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _RayLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _RayPipeline, nullptr);
}
