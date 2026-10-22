#include "tasks.hpp"
#include "utils.hpp"
#include "vulkan_objects.hpp"
#include "gui.hpp"

#include <volk.h>
#include <vector>
#include <vk_mem_alloc.h>

struct Task {
	VkCommandPool command_pool;
	std::vector<VkCommandBuffer> command_buffers;
	std::vector<VkSemaphore> semaphores;
	std::vector<uint64_t> semaphore_values;

	VkQueue queue;
	uint32_t queue_family_index;

	uint32_t max_frames_in_flight;
	uint32_t current_frame;
};

enum ETaskType {
	Graphics,
	Transfer,
	Compute,
};

struct AddImages {
	struct PushConstantRange {
		uint32_t image_width;
		uint32_t image_height;
		uint32_t image_count;
	};

	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
	VkDescriptorSetLayout desc_set_layout;

	// Heap
	struct DescHeap {
		Buffer buffer;
	};

	// Buffer
	struct DescBuffer {
		Buffer buffer;
		VkDeviceSize desc_set_layout_size;
	};

	// Indexing
	struct DescIndexing {
		VkDescriptorPool desc_pool;
		VkDescriptorSet desc_set;
	};

	union {
		DescHeap desc_heap;
		DescBuffer desc_buffer;
		DescIndexing desc_indexing;
	};
};

static Task graphics_task = {};
static Task compute_task = {};
static Task transfer_task = {};

static std::vector<Image> _color_targets = {};
static uint32_t swapchain_image_index = 0;

static AddImages add_images = {};

void Task_Create(Task& task, VkQueue queue, const uint32_t queue_family_index, const uint32_t max_frames_in_flight, const std::string& name)
{
	const VkSemaphoreTypeCreateInfo sem_type_ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0,
	};

	const VkSemaphoreCreateInfo sem_ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &sem_type_ci,
	};

	VkSemaphoreSignalInfo sem_sig_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
		.value = 1,
	};

	const VkCommandPoolCreateInfo cmd_pool_ci = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = queue_family_index,
	};

	VK_CHECK("create command pool", vkCreateCommandPool(Device_Device(), &cmd_pool_ci, nullptr, &task.command_pool));

#ifdef _DEBUG
	Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(task.command_pool), name + " command pool");
#endif

	task.max_frames_in_flight = max_frames_in_flight;
	task.command_buffers.resize(task.max_frames_in_flight);
	task.semaphores.resize(task.max_frames_in_flight);
	task.semaphore_values.resize(task.max_frames_in_flight, 1);
	task.queue = queue;

	const VkCommandBufferAllocateInfo allocate_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = task.command_pool,
		.commandBufferCount = task.max_frames_in_flight,
	};

	VK_CHECK("allocate command buffers", vkAllocateCommandBuffers(Device_Device(), &allocate_info, task.command_buffers.data()));

	for (uint32_t i = 0; i < task.max_frames_in_flight; ++i)
	{
		VK_CHECK("create semaphore", vkCreateSemaphore(Device_Device(), &sem_ci, nullptr, task.semaphores.data() + i));

#ifdef _DEBUG
		Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(task.semaphores.at(i)), name + " semaphore " + std::to_string(i));
		Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<uint64_t>(task.command_buffers.at(i)), name + " command buffer " + std::to_string(i));
#endif
		sem_sig_info.semaphore = task.semaphores.at(i);
		VK_CHECK("signal semaphore", vkSignalSemaphore(Device_Device(), &sem_sig_info));
	}
}

VkCommandBuffer Task_CommandBuffer(const Task& task)
{
	return task.command_buffers.at(task.current_frame);
}

VkSemaphore Task_Semaphore(const Task& task)
{
	return task.semaphores.at(task.current_frame);
}

uint64_t& Task_SemaphoreValue(Task& task)
{
	return task.semaphore_values[task.current_frame];
}

void Task_Start(Task& task)
{
	VkSemaphore semaphore = Task_Semaphore(task);
	uint64_t& value = Task_SemaphoreValue(task);

	const VkSemaphoreWaitInfo wait_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &semaphore,
		.pValues = &value,
	};

	VK_CHECK("wait semaphores", vkWaitSemaphores(Device_Device(), &wait_info, UINT64_MAX));

	const VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK("start task", vkBeginCommandBuffer(Task_CommandBuffer(task), &begin_info));
}

void Task_End(const Task& task)
{
	VK_CHECK("end task", vkEndCommandBuffer(Task_CommandBuffer(task)));
}

void Task_Submit(Task& task, std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos)
{
	wait_sem_infos.push_back(
		VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Task_Semaphore(task),
			.value = Task_SemaphoreValue(task),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		});

	const VkCommandBufferSubmitInfo cmd_buff_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = Task_CommandBuffer(task),
}
	};

	sig_sem_infos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Task_Semaphore(task),
			.value = ++Task_SemaphoreValue(task),
			.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		});

	const VkSubmitInfo2 submit_infos[] = {
		{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = static_cast<uint32_t>(wait_sem_infos.size()),
		.pWaitSemaphoreInfos = wait_sem_infos.data(),
		.commandBufferInfoCount = _countof(cmd_buff_infos),
		.pCommandBufferInfos = cmd_buff_infos,
		.signalSemaphoreInfoCount = static_cast<uint32_t>(sig_sem_infos.size()),
		.pSignalSemaphoreInfos = sig_sem_infos.data(),
},
	};

	VK_CHECK("queue submit", vkQueueSubmit2(task.queue, _countof(submit_infos), submit_infos, VK_NULL_HANDLE));
}

void Task_NextFrame(Task& task)
{
	task.current_frame = (task.current_frame + 1) % task.max_frames_in_flight;
}

void Task_Destroy(Task task)
{
	if (Device_Device() != VK_NULL_HANDLE)
	{
		if (task.command_pool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(Device_Device(), task.command_pool, nullptr);
		}

		for (const auto sem : task.semaphores)
		{
			vkDestroySemaphore(Device_Device(), sem, nullptr);
		}
	}
}

void GraphicsTask_Create(uint32_t max_frames_in_flight)
{
	Task_Create(graphics_task, Device_Graphics_Queue(), PhysicalDeviceData_GraphicsQueueFamilyIndex(), max_frames_in_flight, "graphics task");
}

void GraphicsTask_Begin()
{
	Task_Start(graphics_task);
}

void GraphicsTask_ChangeImageLayout(
	VkImage image,
	VkImageLayout old_layout, VkImageLayout new_layout,
	VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask,
	VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask
)
{
	const VkImageMemoryBarrier2 img_mem_barrs[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask,
			.oldLayout = old_layout,
			.newLayout = new_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1,
},
}
	};

	const VkDependencyInfo dependency_info = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = _countof(img_mem_barrs),
		.pImageMemoryBarriers = img_mem_barrs,
	};

	vkCmdPipelineBarrier2(Task_CommandBuffer(graphics_task), &dependency_info);
}

void GraphicsTask_BeginRenderPass(const std::vector<Image>& color_targets, const VkRect2D render_area)
{
	for (const auto& color_target : color_targets)
	{
		GraphicsTask_ChangeImageLayout(
			color_target.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
		);
	}

	const VkClearValue clear_value = {
		.color = {
			.float32 = {(float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, 1},
		}
	};

	std::vector<VkRenderingAttachmentInfo> color_attachments;

	for (const auto& color_target : color_targets)
	{
		color_attachments.push_back({
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = color_target.image_view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clear_value,
			});
	}

	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = render_area,
		.layerCount = 1,
		.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
		.pColorAttachments = color_attachments.data(),
	};

	_color_targets = color_targets;

	vkCmdBeginRendering(Task_CommandBuffer(graphics_task), &rendering_info);
}

void GraphicsTask_RenderGUI()
{
	GUI_Render(Task_CommandBuffer(graphics_task));
}

void GraphicsTask_Render()
{

}

void GraphicsTask_EndRenderPass()
{
	vkCmdEndRendering(Task_CommandBuffer(graphics_task));
}

void GraphicsTask_End()
{
	for (const auto& color_target : _color_targets)
	{
		GraphicsTask_ChangeImageLayout(
			color_target.image,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT
		);
	}

	const VkAcquireNextImageInfoKHR acq_info = {
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = Swapchain_Swapchain(),
		.timeout = UINT64_MAX,
		.semaphore = Swapchain_AcqSigSem(),
		.deviceMask = 1,
	};

	VK_CHECK("acquire next image", vkAcquireNextImage2KHR(Device_Device(), &acq_info, &swapchain_image_index));

	VkImage swapchain_image = Swapchain_Image(swapchain_image_index);

	GraphicsTask_ChangeImageLayout(
		swapchain_image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT
	);

	const VkImageBlit2 regions[] = {
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
			.srcSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1,
			},
			.srcOffsets = {{}, {.x = static_cast<int32_t>(Surface_Extent().width), .y = static_cast<int32_t>(Surface_Extent().height), .z = 1}},
			.dstSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1
			},
			.dstOffsets = {{}, {.x = static_cast<int32_t>(Surface_Extent().width), .y = static_cast<int32_t>(Surface_Extent().height), .z = 1}}
		}
	};

	const VkBlitImageInfo2 blit_info = {
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.srcImage = _color_targets.at(0).image,
		.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.dstImage = swapchain_image,
		.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.regionCount = _countof(regions),
		.pRegions = regions,
	};
	vkCmdBlitImage2(Task_CommandBuffer(graphics_task), &blit_info);
	GraphicsTask_ChangeImageLayout(
		swapchain_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0
	);

	Task_End(graphics_task);
}

void GraphicsTask_Submit(std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos)
{
	wait_sem_infos.push_back(
		VkSemaphoreSubmitInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = Swapchain_AcqSigSem(),
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		}
		);

	sig_sem_infos.push_back(
		VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Swapchain_PrsntWaitSem(),
			.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		});

	Task_Submit(graphics_task, wait_sem_infos, sig_sem_infos);
}

void GraphicsTask_Present()
{
	VkSemaphore prsnt_wait_sem = Swapchain_PrsntWaitSem();
	VkSwapchainKHR swapchain = Swapchain_Swapchain();

	const VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &prsnt_wait_sem,
		.swapchainCount = 1,
		.pSwapchains = &swapchain,
		.pImageIndices = &swapchain_image_index,
	};
	VK_CHECK("queue present", vkQueuePresentKHR(graphics_task.queue, &present_info));
}

void GraphicsTask_NextFrame()
{
	Task_NextFrame(graphics_task);
	Swapchain_NextFrame();
}

void GraphicsTask_Destroy()
{
	Task_Destroy(graphics_task);
}

void TransferTask_Create(uint32_t max_frames_in_flight)
{
	Task_Create(transfer_task, Device_Transfer_Queue(), PhysicalDeviceData_TransferQueueFamilyIndex(), max_frames_in_flight, "transfer task");
}

void TransferTask_Begin()
{
	Task_Start(transfer_task);
}

void TransferTask_End()
{
	Task_End(transfer_task);
}

void TransferTask_Submit(std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos)
{
	Task_Submit(transfer_task, wait_sem_infos, sig_sem_infos);
}

void TransferTask_NextFrame()
{
	Task_NextFrame(transfer_task);
}

void TransferTask_Destroy()
{
	Task_Destroy(transfer_task);
}

void ComputeTask_AddImages_Create(const std::vector<Image>& color_targets, const Image& gui_target)
{
	VkShaderModule shader_module = VK_NULL_HANDLE;

	std::string shader_path = std::string(SDL_GetBasePath()).append("shaders\\add_images.slang.spv");

	SDL_IOStream* io_stream = SDL_IOFromFile(shader_path.c_str(), "rb");
	size_t code_size = 0;
	uint32_t* code = reinterpret_cast<uint32_t*>(SDL_LoadFile_IO(io_stream, &code_size, true));

	const VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<size_t>(code_size),
		.pCode = code,
	};

	VK_CHECK("create add images shader module", vkCreateShaderModule(Device_Device(), &create_info, nullptr, &shader_module));
	SDL_free(code);

	const VkPipelineShaderStageCreateInfo shader_stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = shader_module,
		.pName = "main"
	};

	const VkDescriptorSetLayoutBinding dsl_set_0_bindings[] = {
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		},
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		},
		{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = static_cast<uint32_t>(color_targets.size()) * 100,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		},
	};

	const VkDescriptorBindingFlags binding_flags[] = {
		0, 0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
	};

	const VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_ci = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = _countof(binding_flags),
		.pBindingFlags = binding_flags
	};

	VkDescriptorSetLayoutCreateInfo dsl_create_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &binding_flags_ci,
		.bindingCount = _countof(dsl_set_0_bindings),
		.pBindings = dsl_set_0_bindings,
	};

	if (descriptor_mode == EDescriptorMode::Desc_Buffer)
	{
		dsl_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
	}

	VK_CHECK("create add images dsl", vkCreateDescriptorSetLayout(Device_Device(), &dsl_create_info, nullptr, &add_images.desc_set_layout));

	const VkPushConstantRange pc_rngs[] = {
		{
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.size = sizeof(AddImages::PushConstantRange),
		}
	};

	const VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &add_images.desc_set_layout,
		.pushConstantRangeCount = _countof(pc_rngs),
		.pPushConstantRanges = pc_rngs,
	};

	VK_CHECK("create add images pipeline layout", vkCreatePipelineLayout(Device_Device(), &pipeline_layout_create_info, nullptr, &add_images.pipeline_layout));

	const VkComputePipelineCreateInfo create_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = shader_stage_create_info,
			.layout = add_images.pipeline_layout,
		}
	};

	VK_CHECK("create add images pipeline", vkCreateComputePipelines(Device_Device(), VK_NULL_HANDLE, _countof(create_infos), create_infos, nullptr, &add_images.pipeline));

	vkDestroyShaderModule(Device_Device(), shader_module, nullptr);

	if (descriptor_mode == EDescriptorMode::Desc_Heap)
	{

	}
	else if (descriptor_mode == EDescriptorMode::Desc_Buffer)
	{
		VkPhysicalDeviceDescriptorBufferPropertiesEXT desc_buff_props = PhysicalDeviceData_DescBuffProps();
		vkGetDescriptorSetLayoutSizeEXT(Device_Device(), add_images.desc_set_layout, &add_images.desc_buffer.desc_set_layout_size);
		add_images.desc_buffer.buffer = Allocator_CreateBuffer(ALIGNED_SIZE(add_images.desc_buffer.desc_set_layout_size, desc_buff_props.descriptorBufferOffsetAlignment), VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

		VkDeviceSize buff_offset = 0;
		size_t i = 0;
		for (const auto& color_target : color_targets)
		{
			const VkDescriptorImageInfo image_info = {
				.sampler = color_target.sampler,
				.imageView = color_target.image_view,
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			};

			const VkDescriptorGetInfoEXT desc_get_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.data {
					.pStorageImage = &image_info,
				}
			};

			vkGetDescriptorEXT(Device_Device(), &desc_get_info, desc_buff_props.storageImageDescriptorSize, reinterpret_cast<void*>(reinterpret_cast<VkDeviceSize>(add_images.desc_buffer.buffer.allocation->GetMappedData()) + buff_offset));
			buff_offset = (desc_buff_props.storageImageDescriptorSize * i);
			++i;
		}

		const VkDescriptorImageInfo image_info = {
			.sampler = gui_target.sampler,
			.imageView = gui_target.image_view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		const VkDescriptorGetInfoEXT desc_get_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
			.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.data {
				.pStorageImage = &image_info,
			}
		};

		//vkGetDescriptorEXT(Device_Device(), &desc_get_info, desc_buff_props.storageImageDescriptorSize, reinterpret_cast<void*>(reinterpret_cast<VkDeviceSize>(add_images.desc_buffer.buffer.allocation->GetMappedData()) + buff_offset));
		buff_offset = (desc_buff_props.storageImageDescriptorSize + i);
		++i;
	}
	else if (descriptor_mode == EDescriptorMode::Desc_Index)
	{
		const VkDescriptorPoolSize pool_sizes[] = {
			{
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = static_cast<uint32_t>(color_targets.size()) + 1 + 1,
			}
		};

		const VkDescriptorPoolCreateInfo desc_pool_create_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = 1,
			.poolSizeCount = _countof(pool_sizes),
			.pPoolSizes = pool_sizes,
		};
		VK_CHECK("create add images desc pool", vkCreateDescriptorPool(Device_Device(), &desc_pool_create_info, nullptr, &add_images.desc_indexing.desc_pool));

		const uint32_t desc_counts[] = { 1, 1, static_cast<uint32_t>(color_targets.size()) * 5 };

		const VkDescriptorSetVariableDescriptorCountAllocateInfo var_cnt_desc_set_alloc_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
			.descriptorSetCount = 1,
			.pDescriptorCounts = desc_counts,
		};

		const VkDescriptorSetAllocateInfo desc_set_alloc_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = &var_cnt_desc_set_alloc_info,
			.descriptorPool = add_images.desc_indexing.desc_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &add_images.desc_set_layout,
		};
		VK_CHECK("allocate add images desc set", vkAllocateDescriptorSets(Device_Device(), &desc_set_alloc_info, &add_images.desc_indexing.desc_set));

#ifdef _DEBUG
		Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_DESCRIPTOR_SET, reinterpret_cast<uint64_t>(add_images.desc_indexing.desc_set), std::string("AddImages") + " desc set");
		Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_DESCRIPTOR_POOL, reinterpret_cast<uint64_t>(add_images.desc_indexing.desc_pool), std::string("AddImages") + " desc pool");
#endif
	}

#ifdef _DEBUG
	Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, reinterpret_cast<uint64_t>(add_images.desc_set_layout), std::string("AddImages") + " desc set layout");
	Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(add_images.pipeline_layout), std::string("AddImages") + " pipeline layout");
	Utils_SetObjectName(Device_Device(), VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(add_images.pipeline), std::string("AddImages") + " pipeline");
#endif
}

void ComputeTask_AddImages_Destroy()
{
	vkDestroyPipelineLayout(Device_Device(), add_images.pipeline_layout, nullptr);
	vkDestroyDescriptorSetLayout(Device_Device(), add_images.desc_set_layout, nullptr);
	vkDestroyPipeline(Device_Device(), add_images.pipeline, nullptr);

	if (descriptor_mode == EDescriptorMode::Desc_Heap)
	{
		Allocator_DestroyBuffer(add_images.desc_heap.buffer);
	}
	else if (descriptor_mode == EDescriptorMode::Desc_Buffer)
	{
		Allocator_DestroyBuffer(add_images.desc_buffer.buffer);
	}
	else if (descriptor_mode == EDescriptorMode::Desc_Index)
	{
		vkDestroyDescriptorPool(Device_Device(), add_images.desc_indexing.desc_pool, nullptr);
	}
}

void ComputeTask_Create(uint32_t max_frames_in_flight, const std::vector<Image>& color_targets, const Image& gui_target)
{
	Task_Create(compute_task, Device_Compute_Queue(), PhysicalDeviceData_ComputeQueueFamilyIndex(), max_frames_in_flight, "compute task");

	// Create AddImages
	ComputeTask_AddImages_Create(color_targets, gui_target);
}

void ComputeTask_Begin()
{
	Task_Start(compute_task);
}

void ComputeTask_AddImages_Execute(const std::vector<Image>& images)
{
	// Bind add images compute pipeline
	// Bind descriptors
	// dispatch
}

void ComputeTask_End()
{
	Task_End(compute_task);
}

void ComputeTask_Submit(std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos)
{
	Task_Submit(compute_task, wait_sem_infos, sig_sem_infos);
}

void ComputeTask_NextFrame()
{
	Task_NextFrame(compute_task);
}

void ComputeTask_Destroy()
{
	Task_Destroy(compute_task);

	// Destroy AddImages
	ComputeTask_AddImages_Destroy();
}