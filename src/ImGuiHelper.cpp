
#include "ImGuiHelper.h"

//imgui include
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

//app include
#include "Scene.h"
#include "GraphicsAPIManager.h"

/*===== Init =====*/

void InitImGuiVulkan(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource)
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

		VK_CALL_PRINT(vkCreateDescriptorPool(GAPI.VulkanDevice, &pool_info, nullptr, &ImGuiResource.ImGuiDescriptorPool));
	}

	//basically, no attachement to the UI renderpass
	VkAttachmentDescription attachment = {};
	attachment.format			= GAPI.VulkanSurfaceFormat.format;
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
	VK_CALL_PRINT(vkCreateRenderPass(GAPI.VulkanDevice, &renderpass_info, nullptr, &ImGuiResource.ImGuiRenderPass));

	ImGui_ImplGlfw_InitForVulkan(GAPI.VulkanWindow, true);

	ResetImGuiResource(GAPI, ImGuiResource);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance			= GAPI.VulkanInterface;
	init_info.PhysicalDevice	= GAPI.VulkanGPU;
	init_info.Device			= GAPI.VulkanDevice;
	init_info.QueueFamily		= GAPI.VulkanQueueFamily;
	init_info.Queue				= GAPI.RuntimeHandle.VulkanQueues[1];//Imgui has its own command queue to avoid mixing drawing and UI.
	init_info.PipelineCache		= VK_NULL_HANDLE;
	init_info.DescriptorPool	= ImGuiResource.ImGuiDescriptorPool;
	init_info.RenderPass		= ImGuiResource.ImGuiRenderPass;
	init_info.Subpass			= 0;
	init_info.MinImageCount		= 3;
	init_info.ImageCount		= GAPI.NbVulkanFrames;
	init_info.MSAASamples		= VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator			= nullptr;
	init_info.CheckVkResultFn	= nullptr;
	ImGui_ImplVulkan_Init(&init_info);
}

/*===== Resize Resource =====*/

bool ResetImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource)
{
	VkResult result = VK_SUCCESS;


	VK_CLEAR_ARRAY(ImGuiResource.ImguiFrameBuffer, GAPI.NbVulkanFrames, vkDestroyFramebuffer, GAPI.VulkanDevice);

	if (ImGuiResource.ImGuiCommandBuffer != nullptr)
		vkFreeCommandBuffers(GAPI.VulkanDevice, GAPI.VulkanCommandPool[1], GAPI.NbVulkanFrames, *ImGuiResource.ImGuiCommandBuffer);

	VK_CLEAR_ARRAY(ImGuiResource.VulkanHasDrawnUI, GAPI.NbVulkanFrames, vkDestroySemaphore, GAPI.VulkanDevice);


	VkImageView backbuffers[1];
	VkFramebufferCreateInfo framebuffer_info = {};
	framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_info.renderPass = ImGuiResource.ImGuiRenderPass;
	framebuffer_info.attachmentCount = 1;
	framebuffer_info.pAttachments	= backbuffers;
	framebuffer_info.width			= GAPI.VulkanWidth;
	framebuffer_info.height			= GAPI.VulkanHeight;
	framebuffer_info.layers			= 1;

	ImGuiResource.ImguiFrameBuffer = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * GAPI.NbVulkanFrames);
	ImGuiResource.VulkanHasDrawnUI = (VkSemaphore*)malloc(sizeof(VkSemaphore) * GAPI.NbVulkanFrames);

	for (uint32_t i = 0; i < GAPI.NbVulkanFrames; i++)
	{
		backbuffers[0] = GAPI.VulkanBackColourBuffers[i];
		VK_CALL_PRINT(vkCreateFramebuffer(GAPI.VulkanDevice, &framebuffer_info, nullptr, &ImGuiResource.ImguiFrameBuffer[i]));

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VK_CALL_PRINT(vkCreateSemaphore(GAPI.VulkanDevice, &semaphoreInfo, nullptr, &ImGuiResource.VulkanHasDrawnUI[i]));
	}

	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = GAPI.VulkanCommandPool[1];
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = GAPI.NbVulkanFrames;

		ImGuiResource.ImGuiCommandBuffer = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * GAPI.NbVulkanFrames);
		VK_CALL_PRINT(vkAllocateCommandBuffers(GAPI.VulkanDevice, &allocInfo, *ImGuiResource.ImGuiCommandBuffer));
	}

	return result == VK_SUCCESS;
}

/*===== Draw =====*/

void BeginDrawUIWindow(const GraphicsAPIManager& GAPI, SmartLoopArray<class Scene*>& scenes, AppWideContext& AppContext)
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
		camera_changed |= ImGui::SliderFloat("Near Plane", &AppContext.near_plane, 0.0f, 4000.0f);
		camera_changed |= ImGui::SliderFloat("Far Plane", &AppContext.far_plane, 0.0f, 4000.0f);
		camera_changed |= ImGui::SliderFloat("Cam Translate Speed", &AppContext.camera_translate_speed, 0.1f, 1000.0f);
		camera_changed |= ImGui::SliderFloat("Cam Rot Speed", &AppContext.camera_rotational_speed, 0.1f, 1000.0f);

		if (camera_changed)
		{
			AppContext.proj_mat = ro_perspective_proj(GAPI.VulkanWidth, GAPI.VulkanHeight, AppContext.fov * RADIANS_TO_DEGREES, AppContext.near_plane, AppContext.far_plane);
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


void FinishDrawUIWindow(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource, AppWideContext& AppContext)
{
	VkResult err;

	{
		err = vkResetCommandBuffer(ImGuiResource.ImGuiCommandBuffer[GAPI.RuntimeHandle.VulkanCurrentFrame], 0);
		//check_vk_result(err);
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		err = vkBeginCommandBuffer(ImGuiResource.ImGuiCommandBuffer[GAPI.RuntimeHandle.VulkanCurrentFrame], &info);
		//	check_vk_result(err);
	}
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.renderPass = ImGuiResource.ImGuiRenderPass;
		info.framebuffer = ImGuiResource.ImguiFrameBuffer[GAPI.RuntimeHandle.VulkanFrameIndex];
		info.renderArea.extent.width = GAPI.VulkanWidth;
		info.renderArea.extent.height = GAPI.VulkanHeight;
		info.clearValueCount = 0;
		//info.pClearValues				= &imgui.ImGuiWindow.ClearValue;
		vkCmdBeginRenderPass(ImGuiResource.ImGuiCommandBuffer[GAPI.RuntimeHandle.VulkanCurrentFrame], &info, VK_SUBPASS_CONTENTS_INLINE);
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
	ImGui_ImplVulkan_RenderDrawData(draw_data, ImGuiResource.ImGuiCommandBuffer[GAPI.RuntimeHandle.VulkanCurrentFrame]);


	// Submit command buffer
	vkCmdEndRenderPass(ImGuiResource.ImGuiCommandBuffer[GAPI.RuntimeHandle.VulkanCurrentFrame]);
	{
		VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &GAPI.RuntimeHandle.GetCurrentHasPresentedSemaphore();
		info.pWaitDstStageMask = &wait_stage;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &ImGuiResource.ImGuiCommandBuffer[GAPI.RuntimeHandle.VulkanCurrentFrame];
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &ImGuiResource.VulkanHasDrawnUI[GAPI.RuntimeHandle.VulkanCurrentFrame];

		err = vkEndCommandBuffer(ImGuiResource.ImGuiCommandBuffer[GAPI.RuntimeHandle.VulkanCurrentFrame]);
		//check_vk_result(err);
		err = vkQueueSubmit(GAPI.RuntimeHandle.VulkanQueues[1], 1, &info, GAPI.RuntimeHandle.GetCurrentIsDrawingFence());
		//check_vk_result(err);
	}
}



/*===== Clear =====*/

void ClearImGuiResource(const GraphicsAPIManager& GAPI, ImGuiResource& ImGuiResource)
{

	VK_CLEAR_ARRAY(ImGuiResource.ImguiFrameBuffer, GAPI.NbVulkanFrames, vkDestroyFramebuffer, GAPI.VulkanDevice);

	if (ImGuiResource.ImGuiCommandBuffer != nullptr)
		vkFreeCommandBuffers(GAPI.VulkanDevice, GAPI.VulkanCommandPool[1], GAPI.NbVulkanFrames, *ImGuiResource.ImGuiCommandBuffer);

	VK_CLEAR_ARRAY(ImGuiResource.VulkanHasDrawnUI, GAPI.NbVulkanFrames, vkDestroySemaphore, GAPI.VulkanDevice);

	vkDestroyDescriptorPool(GAPI.VulkanDevice, ImGuiResource.ImGuiDescriptorPool, nullptr);

	if (ImGuiResource.ImGuiRenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(GAPI.VulkanDevice, ImGuiResource.ImGuiRenderPass, nullptr);
	}
}