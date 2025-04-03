
#include "ImGuiHelper.h"

//imgui include
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

//app include
#include "Scene.h"
#include "GraphicsAPIManager.h"

using namespace ImGuiHelper;

/*===== Init =====*/

void ImGuiHelper::InitImGuiVulkan(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource)
{
	VkResult result = VK_SUCCESS;

	//create a GPU Buffer for ImGUI textures
	{

		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1;
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI._VulkanDevice, &pool_info, nullptr, &ImGuiResource._ImGuiDescriptorPool));
	}

	//basically, no attachement to the UI renderpass
	VkAttachmentDescription attachment = {};
	attachment.format			= GAPI._VulkanSurfaceFormat.format;
	attachment.samples			= VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.storeOp			= VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	//no colour attachment either
	VkAttachmentReference colour_attachment = {};
	colour_attachment.attachment	= 0;
	colour_attachment.layout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//UI is a rasterized graphic pass
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &colour_attachment;

	//and it usually comes at the end of other graphics pass and directly writes on the framebuffer
	VkSubpassDependency dependency = {};
	dependency.srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass		= 0;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask	= 0;
	dependency.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderpass_info = {};
	renderpass_info.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpass_info.attachmentCount = 1;
	renderpass_info.pAttachments	= &attachment;
	renderpass_info.subpassCount	= 1;
	renderpass_info.pSubpasses		= &subpass;
	renderpass_info.dependencyCount = 1;
	renderpass_info.pDependencies	= &dependency;
	VK_CALL_PRINT(vkCreateRenderPass(GAPI._VulkanDevice, &renderpass_info, nullptr, &ImGuiResource._ImGuiRenderPass));

	ImGui_ImplGlfw_InitForVulkan(GAPI._VulkanWindow, true);

	ImGuiHelper::ResetImGuiResource(GAPI, ImGuiResource);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance			= GAPI._VulkanInterface;
	init_info.PhysicalDevice	= GAPI._VulkanGPU;
	init_info.Device			= GAPI._VulkanDevice;
	init_info.QueueFamily		= GAPI._vk_queue_family;
	init_info.Queue				= GAPI._RuntimeHandle._VulkanQueues[1];//Imgui has its own command queue to avoid mixing drawing and UI.
	init_info.PipelineCache		= VK_NULL_HANDLE;
	init_info.DescriptorPool	= ImGuiResource._ImGuiDescriptorPool;
	init_info.RenderPass		= ImGuiResource._ImGuiRenderPass;
	init_info.Subpass			= 0;
	init_info.MinImageCount		= 3;
	init_info.ImageCount		= GAPI._nb_vk_frames;
	init_info.MSAASamples		= VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator			= nullptr;
	init_info.CheckVkResultFn	= nullptr;
	ImGui_ImplVulkan_Init(&init_info);
}

/*===== Resize Resource =====*/

bool ImGuiHelper::ResetImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource)
{
	VkResult result = VK_SUCCESS;


	VK_CLEAR_ARRAY(ImGuiResource._ImguiFrameBuffer, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);

	if (ImGuiResource._ImGuiCommandBuffer != nullptr)
		vkFreeCommandBuffers(GAPI._VulkanDevice, GAPI._VulkanCommandPool[1], GAPI._nb_vk_frames, *ImGuiResource._ImGuiCommandBuffer);
	ImGuiResource._ImGuiCommandBuffer.Clear();

	VK_CLEAR_ARRAY(ImGuiResource._VulkanHasDrawnUI, GAPI._nb_vk_frames, vkDestroySemaphore, GAPI._VulkanDevice);


	VkImageView backbuffers[1];
	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = ImGuiResource._ImGuiRenderPass;
	framebuffer_info.attachmentCount = 1;
	framebuffer_info.pAttachments	= backbuffers;
	framebuffer_info.width			= GAPI._vk_width;
	framebuffer_info.height			= GAPI._vk_height;
	framebuffer_info.layers			= 1;

	ImGuiResource._ImguiFrameBuffer = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * GAPI._nb_vk_frames);
	ImGuiResource._VulkanHasDrawnUI = (VkSemaphore*)malloc(sizeof(VkSemaphore) * GAPI._nb_vk_frames);

	for (uint32_t i = 0; i < GAPI._nb_vk_frames; i++)
	{
		backbuffers[0] = GAPI._VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI._VulkanDevice, &framebuffer_info, nullptr, &ImGuiResource._ImguiFrameBuffer[i]));

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VK_CALL_PRINT(vkCreateSemaphore(GAPI._VulkanDevice, &semaphoreInfo, nullptr, &ImGuiResource._VulkanHasDrawnUI[i]));
	}

	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = GAPI._VulkanCommandPool[1];
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = GAPI._nb_vk_frames;

		ImGuiResource._ImGuiCommandBuffer.Alloc(GAPI._nb_vk_frames);
		VK_CALL_PRINT(vkAllocateCommandBuffers(GAPI._VulkanDevice, &allocInfo, *ImGuiResource._ImGuiCommandBuffer));
	}

	return result == VK_SUCCESS;
}

/*===== Draw =====*/

void ImGuiHelper::BeginDrawUIWindow(const GraphicsAPIManager& GAPI, ScopedLoopArray<Scene*>& scenes, AppWideContext& AppContext)
{
	if (!AppContext.ui_visible)
		return;

	//open the app wide control panel
	if (!ImGui::Begin("Raytraced-CelShading Control Panel", &AppContext.ui_visible, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("%f FPS", 1.0f / AppContext.delta_time);

	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Current Scene"))
		{
			for (uint32_t i = 0; i < scenes.Nb(); i++)
			{
				if (ImGui::MenuItem(scenes[i]->Name(), NULL))
				{
					AppContext.scene_index = i;
					AppContext.in_camera_menu = false;
				}
			}

			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Settings"))
		{
			ImGui::MenuItem("Camera Setings", NULL, &AppContext.in_camera_menu);
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	if (AppContext.in_camera_menu)
	{
		bool camera_changed = false;

		camera_changed |= ImGui::SliderAngle("Camera Field Of View", &AppContext.fov, 0.0f, 180.0f);
		camera_changed |= ImGui::SliderFloat("Near Plane", &AppContext.near_plane, 0.0f, 40.0f);
		camera_changed |= ImGui::SliderFloat("Far Plane", &AppContext.far_plane, 0.0f, 4000.0f);
		camera_changed |= ImGui::SliderFloat("Cam Translate Speed", &AppContext.camera_translate_speed, 0.1f, 1000.0f);
		camera_changed |= ImGui::SliderFloat("Cam Rot Speed", &AppContext.camera_rotational_speed, 0.1f, 1000.0f);

		if (camera_changed)
		{
			AppContext.proj_mat = perspective_proj(GAPI._vk_width, GAPI._vk_height, AppContext.fov, AppContext.near_plane, AppContext.far_plane);
		}
		return;
	}

	// 1. allow to change scene
	if (ImGui::Button("<"))
		AppContext.scene_index = AppContext.scene_index - 1 < 0 ? scenes.Nb() - 1 : AppContext.scene_index - 1;
	ImGui::SameLine();
	ImGui::SliderInt("Scene Index", (int32_t*)&AppContext.scene_index, 0, scenes.Nb() - 1);//uint32_t* to in32_t would ne dangerous if we would have enough scenes that we would reach the most signifant bit, which we'll never reach
	ImGui::SameLine();
	if (ImGui::Button(">"))
		AppContext.scene_index = ++AppContext.scene_index % scenes.Nb();

	//special case : window was opened but this frame it closes, we still need to render the last frame of the UI window
	if (!AppContext.ui_visible)
		ImGui::End();

}


void ImGuiHelper::FinishDrawUIWindow(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource, AppWideContext& AppContext)
{
	VkResult err;

	{
		err = vkResetCommandBuffer(ImGuiResource._ImGuiCommandBuffer[GAPI._RuntimeHandle._vk_current_frame], 0);
		//check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(ImGuiResource._ImGuiCommandBuffer[GAPI._RuntimeHandle._vk_current_frame], &info);
		//	check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass					= ImGuiResource._ImGuiRenderPass;
		info.framebuffer				= ImGuiResource._ImguiFrameBuffer[GAPI._RuntimeHandle._vk_frame_index];
		info.renderArea.extent.width	= GAPI._vk_width;
		info.renderArea.extent.height	= GAPI._vk_height;
		info.clearValueCount = 0;
		//info.pClearValues				= &imgui.ImGuiWindow.ClearValue;
		vkCmdBeginRenderPass(ImGuiResource._ImGuiCommandBuffer[GAPI._RuntimeHandle._vk_current_frame], &info, VK_SUBPASS_CONTENTS_INLINE);
	}


	if (AppContext.ui_visible)
		ImGui::End();

	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();

	if (draw_data->DisplaySize.x <= 0 && draw_data->DisplaySize.y <= 0)
	{
		AppContext.ui_visible = false;
		return;
	}

	// Record dear imgui primitives into command buffer
	ImGui_ImplVulkan_RenderDrawData(draw_data, ImGuiResource._ImGuiCommandBuffer[GAPI._RuntimeHandle._vk_current_frame]);


	// Submit command buffer
	vkCmdEndRenderPass(ImGuiResource._ImGuiCommandBuffer[GAPI._RuntimeHandle._vk_current_frame]);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &GAPI._RuntimeHandle.GetCurrentHasPresentedSemaphore();
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &ImGuiResource._ImGuiCommandBuffer[GAPI._RuntimeHandle._vk_current_frame];
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &ImGuiResource._VulkanHasDrawnUI[GAPI._RuntimeHandle._vk_current_frame];

		err = vkEndCommandBuffer(ImGuiResource._ImGuiCommandBuffer[GAPI._RuntimeHandle._vk_current_frame]);
		//check_vk_result(err);
		err = vkQueueSubmit(GAPI._RuntimeHandle._VulkanQueues[1], 1, &info, GAPI._RuntimeHandle.GetCurrentIsDrawingFence());
		//check_vk_result(err);
	}
}



/*===== Clear =====*/

void ImGuiHelper::ClearImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource)
{

	VK_CLEAR_ARRAY(ImGuiResource._ImguiFrameBuffer, GAPI._nb_vk_frames, vkDestroyFramebuffer, GAPI._VulkanDevice);

	if (ImGuiResource._ImGuiCommandBuffer != nullptr)
		vkFreeCommandBuffers(GAPI._VulkanDevice, GAPI._VulkanCommandPool[1], GAPI._nb_vk_frames, *ImGuiResource._ImGuiCommandBuffer);

	VK_CLEAR_ARRAY(ImGuiResource._VulkanHasDrawnUI, GAPI._nb_vk_frames, vkDestroySemaphore, GAPI._VulkanDevice);

	vkDestroyDescriptorPool(GAPI._VulkanDevice, ImGuiResource._ImGuiDescriptorPool, nullptr);

	if (ImGuiResource._ImGuiRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(GAPI._VulkanDevice, ImGuiResource._ImGuiRenderPass, nullptr);
	}
}


/*===== UI Methods ======*/


bool ImGuiHelper::TransformUI(const char* label, Transform& trs, bool& changed)
{

	if (ImGui::CollapsingHeader(label))
	{
		ImGui::BeginGroup();


		changed |= ImGui::SliderFloat3("Postion",	trs.pos.scalar, -MAX_TRANSLATE, MAX_TRANSLATE);
		changed |= ImGui::SliderFloat3("Rotation", trs.rot.scalar, -MAX_ROT, MAX_ROT);
		changed |= ImGui::SliderFloat3("Scale",	trs.scale.scalar, 0.0f, MAX_SCALE, "%0.01f");

		ImGui::EndGroup();

		return true;

	}

	return false;
}

bool ImGuiHelper::RaytracingParamsUI(const char* label, RaytracingParams& rtParams, bool& changed)
{

	if (ImGui::CollapsingHeader(label))
	{
		//Sample nb and depth
		changed |= ImGui::SliderInt("SampleNb", (int*)&rtParams._pixel_sample_nb, 1, 250);
		changed |= ImGui::SliderInt("Max Bounce Depth", (int*)&rtParams._max_depth, 1, 50);

		//background grdient simulating a sky
		if (ImGui::CollapsingHeader("Background Gradient"))
		{
			ImGui::BeginGroup();

			//we use the 4th component of our vector to act as a boolean for turning on and off our gradient.
			bool useBackgroundGradient = rtParams._background_gradient_top.w == 1.0f;
				
			ImGui::Checkbox("Use Background Gradient", &useBackgroundGradient);

			// we'll make both color picker half of the size
			float width = ImGui::CalcItemWidth() / 2.0f;

			//show the color picker for the top
			ImGui::PushItemWidth(width);
			changed |= ImGui::ColorPicker4("Top", rtParams._background_gradient_top.scalar);
			ImGui::PopItemWidth();

			//we put it on the same line 
			ImGui::SameLine();

			//show the top color picker
			ImGui::PushItemWidth(width);
			changed |= ImGui::ColorPicker4("Bottom", rtParams._background_gradient_bottom.scalar);
			ImGui::PopItemWidth();

			ImGui::EndGroup();

			rtParams._background_gradient_top.w = useBackgroundGradient ? 1.0f : 0.0f;

		}

		return true;
	}

	return false;
}

bool ImGuiHelper::LightUI(const char* label, Light& light, bool& changed)
{

	if (ImGui::CollapsingHeader(label))
	{
		if (ImGui::Button("Directionnal"))
			light._LightType = LightType::DIRECTIONNAL;
		ImGui::SameLine();
		if (ImGui::Button("Sphere"))
			light._LightType = LightType::SPHERE;

		switch (light._LightType)
		{
			case LightType::DIRECTIONNAL:
				changed |= ImGui::SliderFloat3("Light Direction", light._dir.scalar, LIGHT_DIR_EDIT_MIN, LIGHT_DIR_EDIT_MAX);
				break;
			case LightType::SPHERE:
				changed |= ImGui::SliderFloat3("Light Position", light._pos.scalar, -LIGHT_POS_EDIT_MAX, LIGHT_POS_EDIT_MAX);
				changed |= ImGui::SliderFloat("Light Radius", &light._radius, -LIGHT_RADIUS_EDIT_MAX, LIGHT_RADIUS_EDIT_MAX);
				break;
			default:
				printf("error ImGuiHelper::LightUI : Light is something that does not exist");
			return false;
		}

		changed |= ImGui::ColorPicker3("Color", light._color.scalar);

		return true;

	}


	return false;
}

bool ImGuiHelper::CelParamsUI(const char* label, CelParams& celParams, bool& changed)
{
	if (ImGui::CollapsingHeader(label))
	{
		changed |= ImGui::SliderFloat("Specular Glossiness", &celParams._specGlossiness, 0.0f, MAX_CEL_SPECULAR_GLOSS);
		changed |= ImGui::SliderFloat2("Diffuse Step", celParams._celDiffuseStep.scalar, 0.0f,MAX_CEL_DIFFUSE_STEP, "%5f");
		changed |= ImGui::SliderFloat2("Specular Step", celParams._celSpecStep.scalar, 0.0f, MAX_CEL_SPECULAR_STEP, "%5f");

		return true;

	}


	return false;
}
