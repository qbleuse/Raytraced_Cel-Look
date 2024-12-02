#include "RaytraceGPU.h"

//imgui include
#include "imgui.h"

//include app class
#include "GraphicsAPIManager.h"
#include "VulkanHelper.h"

/*==== Prepare =====*/

void RaytraceGPU::PrepareVulkanProps(GraphicsAPIManager& GAPI, VkShaderModule& RayGenShader, VkShaderModule& MissShader, VkShaderModule& HitShader)
{
	/*===== MODEL LOADING & AS BUILDING ======*/

	//load the model
	VulkanHelper::LoadObjFile(GAPI._VulkanUploader, "../../../media/teapot/teapot.obj", _RayModel._Meshes);

	//create bottom level AS from model
	VulkanHelper::CreateRaytracedGeometryFromMesh(GAPI._VulkanUploader, _RayBottomAS, _RayModel._Meshes);

	//create Top level AS from geometry with identity matrix
	VulkanHelper::UploadRaytracedModelFromGeometry(GAPI._VulkanUploader, _RayTopAS, identity(), _RayBottomAS);

	/*===== DESCRIBE SHADER STAGE AND GROUPS =====*/

	//the shader stages we'll give to the pipeline
	VolatileMemoryHeap<VkPipelineShaderStageCreateInfo> shaderStages{ static_cast<VkPipelineShaderStageCreateInfo*>(alloca(sizeof(VkPipelineShaderStageCreateInfo) * 3)) };
	memset(*shaderStages, 0, sizeof(VkPipelineShaderStageCreateInfo) * 3);
	//the shader groups, to allow the pipeline to create the shader table
	VolatileMemoryHeap<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{ static_cast<VkRayTracingShaderGroupCreateInfoKHR*>(alloca(sizeof(VkRayTracingShaderGroupCreateInfoKHR) * 3)) };
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
		samplerLayoutBinding.binding			= 1;
		samplerLayoutBinding.descriptorType		= VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		samplerLayoutBinding.descriptorCount	= 1;
		samplerLayoutBinding.stageFlags			= VK_SHADER_STAGE_RAYGEN_BIT_KHR;


		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType		= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 2;
		VkDescriptorSetLayoutBinding* layoutsBinding = { ASLayoutBinding , colourBackBufferBinding };
		layoutInfo.pBindings = &layoutsBinding;

		VK_CALL_PRINT(vkCreateDescriptorSetLayout(GAPI._VulkanDevice, &layoutInfo, nullptr, &_RayDescriptorLayout));
	}

	{
		//our pipeline layout is the same as the descriptor layout
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType			= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts		= 1_RayDescriptorLayout;
		pipelineLayoutInfo.setLayoutCount	= 1;

		VK_CALL_PRINT(vkCreatePipelineLayout(GAPI._VulkanDevice, &pipelineLayoutInfo, nullptr, &_RayLayout));
	}

	/*===== PIPELINE CREATION =====*/

	//our raytracing pipeline
	vkRaytracingPipelineCreateInfo pipelineInfo{};
	pipelineInfo.stype							= VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	pipelineInfo.stageCount						= 3;
	pipelineInfo.pStages						= *shaderStages;
	pipelineInfo.groupCount						= 3;
	pipelineInfo.pGroups						= *shaderGroups;
	pipelineInfo.maxPipelineRayRecursionDepth	= 10;//for the moment we'll hardcode the value, we'll see if we'll make it editable
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
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, _RayShaderBindingBuffer, _RayShaderBindingMemory);

		//get back handle from shader group created in pipeline
		MultipleVolatileMemory<void> shaderGroupHandle{ alloca(3 * startSizeAligned) };
		vkGetRayTracingShaderGroupHandlesKHR(GAPI._VulkanDevice, _RayPipeline, 0, 3, 3 * startSizeAligned, *shaderGroupHandle);

		//mapping memory to buffer
		void* CPUSBTBufferMap;
		vkMapMemory(GAPI._VulkanDevice, _RayShaderBindingMemory, 0, 3 * startSizeAligned, nullptr, &CPUSBTBufferMap);

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

void RaytraceGPU::PrepareVulkanScripts(class GraphicsAPIManager& GAPI, VkShaderModule& RayGenShader, VkShaderModule& MissShader, VkShaderModule& HitShader)
{
	//define ray gen shader
	const char* ray_gen_shader =
		R"(#version 460
			#etension GL_EXT_ray_tracing : enable

			layout(location = 0) rayPayloadInEXT vec3 payload;

			layout(binding = 0) uniform accelrationStructureExt topLevelAS;
			layout(binding = 1) uniform image2D image;

			void main()
			{
				//finding the pixel we are computing from the ray launch arguments
				vec2 uv = (vec2(gl_LaunchIDEXT.xy) + vec2(0.5)) / vec2(gl_LaunchSizeExt.xy);

				//for the moment, fixed
				vec3 origin = vec3(0, 0, 5);

				//the targets are on a viewport ahead of us by 3
				vec3 target = vec3((uv * 2.0) - 1.0, 2.0);

				//creating a direction for our ray
				vec3 direction = normalize(target - origin);

				payload = vec3(0.0);

				//tracing rays
				traceRayEXT(
					topLevelAS,//Acceleration structure
					gl_RayFlagsOpaqueEXT,//opaque flags (definelty don't want it forever)
					0xFF,//cull mask
					0,0,0,//shader binding table offset, stride, and miss index (as this function can be called from all hit shader)
					origin,//our camera's origin
					0.001,//our collision epsilon
					direction,//the direction of our ray
					10000,//the max length of our ray (this could be changed if we added energy conservation rules, I think, but in our case , it is more or less infinity)
					0
				);
				
				//our recursive call finished, let's write into the image
				imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(payload,0.0));

			})";

	//define miss shader
	const char* miss_shader =
		R"(#version 460
			#etension GL_EXT_ray_tracing : enable

			layout(location = 0) rayPaylodInExt vec3 payload;
			hitAttribEXT vec3 attribs;

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
			#etension GL_EXT_ray_tracing : enable

			layout(location = 0) rayPayloadInEXT vec3 payload;
			hitAttribEXT vec3 attribs;

			void main()
			{
				payload = attribs;

			})";

	CreateVulkanShaders(GAPI._VulkanUploader, RayGenShader, VK_SHADER_STAGE_RAYGEN_BIT_KHR, ray_gen_shader, "Raytrace GPU RayGen");
	CreateVulkanShaders(GAPI._VulkanUploader, MissShader, VK_SHADER_STAGE_MISS_BIT_KHR, miss_shader, "Raytrace GPU Miss");
	CreateVulkanShaders(GAPI._VulkanUploader, HitShader, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closest_hit_shader, "Raytrace GPU Closest");
}

void RaytraceGPU::Prepare(class GraphicsAPIManager& GAPI)
{
	//the shaders needed
	VkShaderModule RaytraceShader;

	//compile the shaders here...
	PrepareVulkanScripts(GAPI, RaytraceShader);
	//then create the pipeline
	PrepareVulkanProps(GAPI, RaytraceShader);

	//a "zero init" of the transform values
	_RayObjData.scale = vec3{ 1.0f, 1.0f, 1.0f };
	_RayObjData.pos = vec3{ 0.0f, 0.0f, 1.0f };
}

/*===== Resize =====*/

void RaytraceGPU::ResizeVulkanResource(class GraphicsAPIManager& GAPI, int32_t width, int32_t height, int32_t old_nb_frames)
{
	VkResult result = VK_SUCCESS;

	/*===== CLEAR RESOURCES ======*/

	//first, we clear previously used resources
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _RayBufferDescriptorPool, nullptr);

	/*===== VIEWPORT AND SCISSORS ======*/

	//filling up the viewport...
	_RayViewport.width = (float)GAPI._vk_width;
	_RayViewport.height = (float)GAPI._vk_height;
	_RayViewport.maxDepth = 1.0f;
	_RayViewport.minDepth = 0.0f;
	_RayViewport.x = 0.0f;
	_RayViewport.y = 0.0f;

	//... and the scissors
	_RayScissors.extent = { (uint32_t)GAPI._vk_width, (uint32_t)GAPI._vk_height };

	/*===== DESCRIPTORS ======*/

	{
		//describing how many descriptor at a time should be allocated
		VkDescriptorPoolSize ASPoolSize{};
		ASPoolSize.type			= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ASPoolSize.descriptorCount = 1;

		VkDescriptorPoolSize imagePoolSize{};
		imagePoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imagePoolSize.descriptorCount = 1;

		//creating our descriptor pool to allocate sets for each frame
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 2;
		VkDescriptorPoolSize poolSizes[2] = {ASPoolSize, imagePoolSize};
		poolInfo.pPoolSizes		= poolSizes;
		poolInfo.maxSets		= GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_RayBufferDescriptorPool))

		//allocating a descriptor set for each of our framebuffer (a uniform buffer per frame)
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType					= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool		= _RayBufferDescriptorPool;
		allocInfo.descriptorSetCount	= GAPI._nb_vk_frames;

		//copying the same layout for every descriptor set
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_RayDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//then create the resource
		_RayBufferDescriptorSet.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_RayBufferDescriptorSet));
	}

	_RayOutput.Alloc(GAPI._nb_vk_frames);

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{

		//Adding our Top Level AS in each descriptor set
		VkWriteDescriptorSetAccelerationStructureKHR ASInfo{};
		ASInfo.pAccelerationStructures		= &_RayTopAS._AccelerationStructure;
		ASInfo.accelerationStructureCount	= 1;
		
		VkWriteDescriptorSet ASDescriptorWrite{};
		ASDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ASDescriptorWrite.dstSet			= _RayBufferDescriptorSet[i];
		ASDescriptorWrite.dstBinding		= 0;
		ASDescriptorWrite.dstArrayElement	= 0;
		ASDescriptorWrite.descriptorType	= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		ASDescriptorWrite.descriptorCount	= 1;
		ASDescriptorWrite.pNext				= &ASInfo;

		//bind the back buffer to each descriptor
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageView = GAPI._VulkanBackColourBuffers[i];
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet imageDescriptorWrite{};
		imageDescriptorWrite.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageDescriptorWrite.dstSet				= _RayBufferDescriptorSet[i];
		imageDescriptorWrite.dstBinding			= 1;
		imageDescriptorWrite.dstArrayElement	= 0;
		imageDescriptorWrite.descriptorType		= VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		imageDescriptorWrite.descriptorCount	= 1;
		imageDescriptorWrite.pImageInfo			= &imageInfo;

		VkWriteDescriptorSet descriptorWrites[2] = { ASDescriptorWrite,  imageDescriptorWrite };
		vkUpdateDescriptorSets(GAPI._VulkanDevice, 2, &descriptorWrites, 0, nullptr);
	}
}


void RaytraceGPU::Resize(class GraphicsAPIManager& GAPI, int32_t old_width, int32_t old_height, uint32_t old_nb_frames)
{
	ResizeVulkanResource(GAPI, old_width, old_height, old_nb_frames);
}

/*===== Act =====*/

void RaytraceGPU::Act(struct AppWideContext& AppContext)
{

	_RayObjData.euler_angles.y += AppContext.delta_time;

	//it will change every frame
	{
		_RayMatBuffer.proj = AppContext.proj_mat;
		_RayMatBuffer.view = AppContext.view_mat;
		_RayMatBuffer.model = scale(_RayObjData.scale.x, _RayObjData.scale.y, _RayObjData.scale.z) * ro_intrinsic_rot(_RayObjData.euler_angles.x, _RayObjData.euler_angles.y, _RayObjData.euler_angles.z) * ro_translate(_RayObjData.pos);
	}

	//UI update
	if (SceneCanShowUI(AppContext))
	{
		ImGui::SliderFloat3("Object Postion", _RayObjData.pos.scalar, -100.0f, 100.0f);
		ImGui::SliderFloat3("Object Rotation", _RayObjData.euler_angles.scalar, -180.0f, 180.0f);
		ImGui::SliderFloat3("Object Scale", _RayObjData.scale.scalar, 0.0f, 1.0f, "%0.01f");
	}

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
	barrier.oldLayout			= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//from nothing
	barrier.newLayout			= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;//to transfer dest
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.image				= ;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_NONE;// making the image accessible for ...
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; // shaders

	//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayPipeline);
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _RayLayout, 0, 1, &_RayDescriptorSet[GAPIHandle._vk_current_frame], 0, nullptr);

	//set viewport and scissors to draw on all screen
	vkCmdSetViewport(commandBuffer, 0, 1, &_RayViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_RayScissors);

	vkCmdTraceRaysKHR(commandBuffer, &_RayShaderBindingAddress[0], &_RayShaderBindingAddress[1], &_RayShaderBindingAddress[2], nullptr, _RayScissors.extent.x, _RayScissors.extent.y, 1);

	// changing the back buffer to be able to being written by pipeline
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;//from nothing
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//to transfer dest
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//could use copy queues in the future
	barrier.image = ;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_NONE;// making the image accessible for ...
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; // shaders

	//layout should change when we go from pipeline doing transfer to frament stage. here it does not really matter because there is pipeline attached.
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

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
	//releasing the "per frames" objects
	VK_CLEAR_ARRAY(_RayOutput, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
	ClearUniformBufferHandle(GAPI._VulkanDevice, _RayMatBufferHandle);

	//the memory allocated by the Vulkan Helper is volatile : it must be explecitly freed !
	ClearModel(GAPI._VulkanDevice, _RayModel);

	//release descriptors
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _RayBufferDescriptorPool, nullptr);
	vkDestroyDescriptorPool(GAPI._VulkanDevice, _RaySamplerDescriptorPool, nullptr);
	_RayBufferDescriptorSet.Clear();
	_RaySamplerDescriptorSet.Clear();
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _RayBufferDescriptorLayout, nullptr);
	vkDestroyDescriptorSetLayout(GAPI._VulkanDevice, _RaySamplerDescriptorLayout, nullptr);

	//release pipeline objects
	vkDestroyPipelineLayout(GAPI._VulkanDevice, _RayLayout, nullptr);
	vkDestroyPipeline(GAPI._VulkanDevice, _RayPipeline, nullptr);
	vkDestroyRenderPass(GAPI._VulkanDevice, _RayRenderPass, nullptr);
}
