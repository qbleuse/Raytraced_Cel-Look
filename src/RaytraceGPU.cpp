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
	VK_CLEAR_ARRAY(_RayOutput, (uint32_t)old_nb_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);
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
		VkDescriptorPoolSize poolUniformSize{};
		poolUniformSize.type			= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolUniformSize.descriptorCount = 1;

		//creating our descriptor pool to allocate sets for each frame
		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount	= 1;
		poolInfo.pPoolSizes		= &poolUniformSize;
		poolInfo.maxSets		= GAPI._nb_vk_frames;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &poolInfo, nullptr, &_RayBufferDescriptorPool))

			//allocating a descriptor set for each of our framebuffer (a uniform buffer per frame)
			VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _RayBufferDescriptorPool;
		allocInfo.descriptorSetCount = GAPI._nb_vk_frames;

		//copying the same layout for every descriptor set
		VkDescriptorSetLayout* layouts = (VkDescriptorSetLayout*)alloca(GAPI._nb_vk_frames * sizeof(VkDescriptorSetLayout));
		for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
			memcpy(&layouts[i], &_RayBufferDescriptorLayout, sizeof(VkDescriptorSetLayout));
		allocInfo.pSetLayouts = layouts;

		//then create the resource
		_RayBufferDescriptorSet.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateDescriptorSets(GAPI._VulkanDevice, &allocInfo, *_RayBufferDescriptorSet));
	}

	/*===== FRAMEBUFFERS ======*/

	//creating the description of the output of our pipeline
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType		= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass	= _RayRenderPass;
	framebufferInfo.width		= GAPI._vk_width;
	framebufferInfo.height		= GAPI._vk_height;
	framebufferInfo.layers		= 1;
	//allocating space for our outputs
	_RayOutput.Alloc(GAPI._nb_vk_frames);

	/*===== UNIFORM BUFFERS ======*/

	//recreate the uniform buffer
	CreateUniformBufferHandle(GAPI._VulkanUploader, _RayMatBufferHandle, GAPI._nb_vk_frames, sizeof(mat4) * 3);

	/*===== LINKING RESOURCES ======*/

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		//link our framebuffers (our pipeline's output) to the backbuffers
		framebufferInfo.attachmentCount = 2;
		VkImageView attachement[2] = { GAPI._VulkanBackColourBuffers[i] , GAPI._VulkanDepthBufferViews[i] };
		framebufferInfo.pAttachments = attachement;
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI._VulkanDevice, &framebufferInfo, nullptr, &_RayOutput[i]))

		//describing our uniform matrices buffer
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer	= _RayMatBufferHandle._GPUBuffer[i];
		bufferInfo.offset	= 0;
		bufferInfo.range	= sizeof(mat4) * 3;

		//then link it to the descriptor
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType			= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet			= _RayBufferDescriptorSet[i];
		descriptorWrite.dstBinding		= 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType	= VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo		= &bufferInfo;

		vkUpdateDescriptorSets(GAPI._VulkanDevice, 1, &descriptorWrite, 0, nullptr);
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
	{
		//Set output and output settings for this render pass.
		VkRenderPassBeginInfo info = {};
		info.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass					= _RayRenderPass;//obviously using this scene's renderpass
		info.framebuffer				= _RayOutput[GAPIHandle._vk_frame_index];//writing in the available backbuffer
		info.renderArea.extent.width	= (uint32_t)_RayViewport.width;//writing on the whole screen
		info.renderArea.extent.height	= (uint32_t)_RayViewport.height;//writing on the whole screen
		info.clearValueCount = 2;

		//clear our outputs (grey for backbuffer, infinity for depth buffer)
		VkClearValue clearValue[2]	= {};
		clearValue[0].color			= { 0.2f, 0.2f, 0.2f, 1.0f };
		clearValue[1].depthStencil	= { 1.0f, 0 };
		info.pClearValues			= clearValue;
		vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
	}

	//the buffer will change every frame as the object rotates every frame
	memcpy(_RayMatBufferHandle._CPUMemoryHandle[GAPIHandle._vk_current_frame], (void*)&_RayMatBuffer, sizeof(mat4) * 3);

	//binding the pipeline to asteriwe our model
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _RayPipeline);
	//binding an available uniform buffer (current frame and frame index may be different, as we can ask for redraw multiple times while the frame is not presenting)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _RayLayout, 0, 1, &_RayBufferDescriptorSet[GAPIHandle._vk_current_frame], 0, nullptr);

	//set viewport and scissors to draw on all screen
	vkCmdSetViewport(commandBuffer, 0, 1, &_RayViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &_RayScissors);

	//draw all meshes, as model may be composed of multiple meshes
	for (uint32_t i = 0; i < _RayModel._Meshes.Nb(); i++)
	{
		//first bind the "material", basically the three compibined sampler descriptors ...
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _RayLayout, 1, 1, &_RayModel._Materials[_RayModel._material_index[i]]._TextureDescriptors, 0, nullptr);
		//... then bind the vertex buffer as described in the input layout of the pipeline ...
		vkCmdBindVertexBuffers(commandBuffer, 0, 3, _RayModel._Meshes[i]._VertexBuffers, (VkDeviceSize*)_RayModel._Meshes[i]._vertex_offsets);
		//... and the index buffers associated with the vertex buffers ...
		vkCmdBindIndexBuffer(commandBuffer, _RayModel._Meshes[i]._Indices, 0, _RayModel._Meshes[i]._indices_type);
		//... before finally drawing, following the index buffer.
		vkCmdDrawIndexed(commandBuffer, _RayModel._Meshes[i]._indices_nb, 1, _RayModel._Meshes[i]._indices_offset, 0, 0);
	}


	//end writing in our backbuffer
	vkCmdEndRenderPass(commandBuffer);
	{
		//the pipeline stage at which the GPU should waait for the semaphore to signal itself
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

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
