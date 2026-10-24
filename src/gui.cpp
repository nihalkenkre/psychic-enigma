#include "gui.hpp"
#include "vulkan_objects.hpp"
#include "utils.hpp"

#include <volk.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL_vulkan.h>

static VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

void GUI_Create(SDL_Window* window)
{
	const VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	const VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 1000,
		.poolSizeCount = std::size(pool_sizes),
		.pPoolSizes = pool_sizes,
	};

	VK_CHECK("create imgui desc pool", vkCreateDescriptorPool(Device_Device(), &pool_info, nullptr, &descriptor_pool));

	ImGui::CreateContext();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().LogFilename = nullptr;

	VkFormat color_attachment_format = VK_FORMAT_R32G32B32A32_SFLOAT;
	ImGui_ImplVulkan_InitInfo imgui_init_info = {
		.Instance = Instance_Instance(),
		.PhysicalDevice = PhysicalDeviceData_PhysicalDevice(),
		.Device = Device_Device(),
		.Queue = Device_Graphics_Queue(),
		.DescriptorPool = descriptor_pool,
		.MinImageCount = Surface_MinImageCount(),
		.ImageCount = Swapchain_MaxFramesInFlight(),
		.PipelineInfoMain = {
			.PipelineRenderingCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &color_attachment_format,
				.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
			},
		},
		.UseDynamicRendering = true,
	};

	SDL_CHECK(ImGui_ImplVulkan_Init(&imgui_init_info));
	SDL_CHECK(ImGui_ImplSDL3_InitForVulkan(window));
}

void GUI_Event(SDL_Event* event)
{
	ImGuiIO& io = ImGui::GetIO();

	ImGui_ImplSDL3_ProcessEvent(event);
}

void GUI_Render(VkCommandBuffer cmd_buff)
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	//ImGui::ShowDemoWindow();

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Backend", "Ctrl+B"))
		{
			ImGui::MenuItem("Vulkan");
			ImGui::MenuItem("DX12");
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buff);

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void GUI_Destroy()
{
	ImGui_ImplSDL3_Shutdown();
	ImGui_ImplVulkan_Shutdown();
	ImGui::DestroyContext();

	if (descriptor_pool)
	{
		vkDestroyDescriptorPool(Device_Device(), descriptor_pool, nullptr);
	}
}
