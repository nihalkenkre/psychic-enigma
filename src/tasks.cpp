#include "tasks.hpp"
#include "utils.hpp"
#include "vulkan_objects.hpp"
#include "gui.hpp"
#include "scene_graph.hpp"

#include <volk.h>
#include <vector>
#include <array>

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

	// Heap
	struct DescHeap {
		Buffer buffer;
	};

	// Buffer
	struct DescBuffer {
		Buffer buffer;
		VkDescriptorSetLayout desc_set_layout;
		VkPipelineLayout pipeline_layout;
		VkDeviceSize desc_set_layout_size;
		std::array<VkDeviceSize, 3> desc_binding_offsets;
	};

	//// Indexing
	//struct DescIndexing {
	//	VkDescriptorPool desc_pool;
	//	VkDescriptorSet desc_set;
	//	VkPipelineLayout pipeline_layout;
	//	VkDescriptorSetLayout desc_set_layout;
	//};

	union {
		DescHeap desc_heap;
		DescBuffer desc_buffer;
		//DescIndexing desc_indexing;
	};
	
	uint8_t max_color_targets = 10;
};

struct BasicShapesNode
{
	struct PushConstantRange {
		VkDeviceAddress position_data_ptr;
		VkDeviceAddress vertices_data_ptr;
		VkDeviceAddress triangles_data_ptr;
		uint32_t triangle_count;
	};

	VkPipeline pipeline;

	struct DescBuffer {
		Buffer buffer;
		VkPipelineLayout pipeline_layout;
	};

	union {
		DescBuffer desc_buffer;
	};

	Buffer mesh_data_buffer;
};

namespace {
	AddImages add_images = {};
	BasicShapesNode basic_shapes_node = {};
	uint32_t swapchain_image_index = 0;
}

void Task_Create(Task& task, VkQueue queue, const uint32_t queue_family_index, const uint32_t max_frames_in_flight, const std::string& name)
{
	VkDevice device = Device_Device();

	constexpr VkSemaphoreTypeCreateInfo sem_type_ci = {
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

	VK_CHECK("create command pool", vkCreateCommandPool(device, &cmd_pool_ci, nullptr, &task.command_pool));

#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(task.command_pool), name + " command pool");
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

	VK_CHECK("allocate command buffers", vkAllocateCommandBuffers(device, &allocate_info, task.command_buffers.data()));

	for (uint32_t i = 0; i < task.max_frames_in_flight; ++i)
	{
		VK_CHECK("create semaphore", vkCreateSemaphore(device, &sem_ci, nullptr, task.semaphores.data() + i));

#ifdef _DEBUG
		Utils_SetObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(task.semaphores.at(i)), name + " semaphore " + std::to_string(i));
		Utils_SetObjectName(device, VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<uint64_t>(task.command_buffers.at(i)), name + " command buffer " + std::to_string(i));
#endif
		sem_sig_info.semaphore = task.semaphores.at(i);
		VK_CHECK("signal semaphore", vkSignalSemaphore(device, &sem_sig_info));
	}
}

void Task_BasicShapesNode_Create()
{
	const VkDevice device = Device_Device();
	VkShaderModule shader_module = VK_NULL_HANDLE;

	const std::string shader_path = std::string(SDL_GetBasePath()).append("shaders\\glsl\\basic_shapes_node.comp.glsl.spv");

	SDL_IOStream* io_stream = SDL_IOFromFile(shader_path.c_str(), "rb");
	size_t code_size = 0;
	uint32_t* code = static_cast<uint32_t*>(SDL_LoadFile_IO(io_stream, &code_size, true));

	const VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<size_t>(code_size),
		.pCode = code,
	};

	VK_CHECK("create basic shapes node shader module", vkCreateShaderModule(device, &create_info, nullptr, &shader_module));
	SDL_free(code);

	const VkPipelineShaderStageCreateInfo stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = shader_module,
		.pName = "main",
	};

	VkPipelineCreateFlags2CreateInfo pipeline_create_flags = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO
	};

	VkComputePipelineCreateInfo pipeline_create_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = stage_create_info,
	};

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{

	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
		constexpr VkPushConstantRange pc_rng = {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.size = sizeof(BasicShapesNode::PushConstantRange),
		};
		
		const VkPipelineLayoutCreateInfo pl_ci = {
			.sType =  VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pc_rng,
		};

		VK_CHECK("create basic nodes shape pipeline layout", vkCreatePipelineLayout(device, &pl_ci, nullptr, &basic_shapes_node.desc_buffer.pipeline_layout));
#ifdef _DEBUG
		Utils_SetObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(basic_shapes_node.desc_buffer.pipeline_layout), "BasicShapesNode pipeline layout");
#endif
		
		pipeline_create_flags.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;
		pipeline_create_info.pNext = &pipeline_create_flags;
		pipeline_create_info.layout = basic_shapes_node.desc_buffer.pipeline_layout;
		
		VK_CHECK("create basic shapes node pipeline", vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &basic_shapes_node.pipeline));
	}
	else if (descriptor_mode == EDescriptorMode::DescIndex)
	{

	}
	
	vkDestroyShaderModule(device, shader_module, nullptr);
	
#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(basic_shapes_node.pipeline), "BasicShapesNode pipeline");
#endif
}

void Task_AddImages_Create()
{
	const VkDevice device = Device_Device();
	VkShaderModule shader_module = VK_NULL_HANDLE;

	const std::string shader_path = std::string(SDL_GetBasePath()).append("shaders\\glsl\\add_images.comp.glsl.spv");

	SDL_IOStream* io_stream = SDL_IOFromFile(shader_path.c_str(), "rb");
	size_t code_size = 0;
	uint32_t* code = static_cast<uint32_t*>(SDL_LoadFile_IO(io_stream, &code_size, true));

	const VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<size_t>(code_size),
		.pCode = code,
	};

	VK_CHECK("create add images shader module", vkCreateShaderModule(device, &create_info, nullptr, &shader_module));
	SDL_free(code);

	const VkPipelineShaderStageCreateInfo stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = shader_module,
		.pName = "main",
	};

	VkPipelineCreateFlags2CreateInfo pipeline_create_flags = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
	};

	VkComputePipelineCreateInfo pipeline_create_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = stage_create_info,
	};

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
		VkPhysicalDeviceDescriptorHeapPropertiesEXT desc_heap_props = PhysicalDeviceData_DescHeapProps();
		VkDeviceSize desc_size = ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment);
		VkDeviceSize total_heap_size = max(
			ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment) * add_images.max_color_targets,
			desc_heap_props.minResourceHeapReservedRange
		);

		add_images.desc_heap.buffer = Allocator_CreateBuffer(
			total_heap_size, VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			"add images descriptor heap"
		);

		pipeline_create_flags.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;
		pipeline_create_info.pNext = &pipeline_create_flags;

		VK_CHECK("create AddImages pipeline", vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &add_images.pipeline));


	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
		const VkDescriptorSetLayoutBinding bindings[] = {
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
					.descriptorCount = add_images.max_color_targets,
					.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				},
		};

		constexpr VkDescriptorBindingFlags binding_flags[] = { 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };

		const VkDescriptorSetLayoutBindingFlagsCreateInfo dsl_bf_ci = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = _countof(binding_flags),
			.pBindingFlags = binding_flags,
		};

		const VkDescriptorSetLayoutCreateInfo dsl_ci = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = &dsl_bf_ci,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
			.bindingCount = _countof(bindings),
			.pBindings = bindings,
		};

		VK_CHECK("create add images desc set layout", vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &add_images.desc_buffer.desc_set_layout));

		constexpr VkPushConstantRange pc_rng = {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.size = sizeof(AddImages::PushConstantRange)
		};

		const VkPipelineLayoutCreateInfo pl_ci = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
			.pSetLayouts = &add_images.desc_buffer.desc_set_layout,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pc_rng,
		};
		VK_CHECK("create add images pipeline layout", vkCreatePipelineLayout(device, &pl_ci, nullptr, &add_images.desc_buffer.pipeline_layout));

		pipeline_create_flags.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_BUFFER_BIT_EXT;
		pipeline_create_info.pNext = &pipeline_create_flags;
		pipeline_create_info.layout = add_images.desc_buffer.pipeline_layout;
		VK_CHECK("create add images pipeline", vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &add_images.pipeline));

		VkDeviceSize desc_set_layout_size = 0;
		vkGetDescriptorSetLayoutSizeEXT(device, add_images.desc_buffer.desc_set_layout, &desc_set_layout_size);

		VkDeviceSize binding_0_offset = 0;
		vkGetDescriptorSetLayoutBindingOffsetEXT(device, add_images.desc_buffer.desc_set_layout, 0, &binding_0_offset);
		VkDeviceSize binding_1_offset = 0;
		vkGetDescriptorSetLayoutBindingOffsetEXT(device, add_images.desc_buffer.desc_set_layout, 1, &binding_1_offset);
		VkDeviceSize binding_2_offset = 0;
		vkGetDescriptorSetLayoutBindingOffsetEXT(device, add_images.desc_buffer.desc_set_layout, 2, &binding_2_offset);

		add_images.desc_buffer.desc_binding_offsets = { binding_0_offset, binding_1_offset, binding_2_offset };
		add_images.desc_buffer.buffer = Allocator_CreateBuffer(
			desc_set_layout_size,
			VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			"AddImages desc buffer"
		);
#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(add_images.desc_buffer.pipeline_layout), "AddImages pipeline layout");
#endif
	}

	vkDestroyShaderModule(device, shader_module, nullptr);
	
#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(add_images.pipeline), "AddImages pipeline");
#endif
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
	return task.semaphore_values.at(task.current_frame);
}

void Task_Begin(Task& task)
{
	VkSemaphore semaphore = Task_Semaphore(task);
	uint64_t value = Task_SemaphoreValue(task);

	const VkSemaphoreWaitInfo wait_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &semaphore,
		.pValues = &value,
	};

	VK_CHECK("wait semaphores", vkWaitSemaphores(Device_Device(), &wait_info, UINT64_MAX));

	constexpr VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	VK_CHECK("start task", vkBeginCommandBuffer(Task_CommandBuffer(task), &begin_info));
}

void Task_BeginRenderPass(const Task& task, const std::vector<Image>& color_targets, const Image& depth_target, const VkRect2D render_area)
{
	for (const auto& color_target : color_targets)
	{
		Task_ChangeImageLayout(
			task,
			color_target.image,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
		);
	}

	Task_ChangeImageLayout(
		task,
		depth_target.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
	);

	const VkClearValue color_clear_value = {
		.color = {
			.float32 = {(float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, 1},
		}
	};

	std::vector<VkRenderingAttachmentInfo> color_attachments;
	color_attachments.reserve(color_targets.size());
	
	for (const auto& color_target : color_targets)
	{
		color_attachments.push_back(
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = color_target.image_view,
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = color_clear_value,
			}
			);
	}

	constexpr VkClearValue depth_clear_value = {
		.depthStencil = {
			.depth = 0.f,
		}
	};

	const VkRenderingAttachmentInfo depth_attachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = depth_target.image_view,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = depth_clear_value,
	};

	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = render_area,
		.layerCount = 1,
		.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
		.pColorAttachments = color_attachments.data(),
		.pDepthAttachment = &depth_attachment,
	};

	vkCmdBeginRendering(Task_CommandBuffer(task), &rendering_info);
}

void Task_RenderSceneGraph(const Task& task)
{
	SceneGraph_Render(Task_CommandBuffer(task));
}

void Task_EndRenderPass(const Task& task)
{
	vkCmdEndRendering(Task_CommandBuffer(task));
}

void Task_BeginGUIRenderPass(const Task& task, const Image& gui_target, const VkRect2D render_area)
{
	Task_ChangeImageLayout(
		task,
		gui_target.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
	);

	constexpr VkClearValue clear_value = {
		.color = {
			.float32 = {0, 0, 0, 0},
		}
	};

	const VkRenderingAttachmentInfo color_attachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = gui_target.image_view,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clear_value
	};

	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = render_area,
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment,
	};

	vkCmdBeginRendering(Task_CommandBuffer(task), &rendering_info);
}

void Task_RenderGUI(const Task& task)
{
	GUI_Render(Task_CommandBuffer(task));
}

void Task_EndGUIRenderPass(const Task& task)
{
	vkCmdEndRendering(Task_CommandBuffer(task));
}

void Task_ExecutionBarrier(const Task& task, const VkPipelineStageFlags2 src_stage_mask, const VkAccessFlags2 src_access_mask, const VkPipelineStageFlags2 dst_stage_mask, const VkAccessFlags2 dst_access_mask)
{
	const VkMemoryBarrier2 mem_barr = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = src_stage_mask,
		.srcAccessMask = src_access_mask,
		.dstStageMask = dst_stage_mask,
		.dstAccessMask = dst_access_mask,
	};

	const VkDependencyInfo dependency_info = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &mem_barr,
	};
	vkCmdPipelineBarrier2(Task_CommandBuffer(task), &dependency_info);
}

void Task_AddImages(const Task& task, const std::vector<Image>& color_targets, const Image& gui_target, const Image& final_image)
{
	VkCommandBuffer cmd_buff = Task_CommandBuffer(task);

	Task_ChangeImageLayout(
		task,
		final_image.image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT
	);

	constexpr VkClearColorValue clear_color = {
		.float32 = {0, 0, 0, 0}
	};

	constexpr VkImageSubresourceRange range = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.levelCount = 1,
		.layerCount = 1
	};

	vkCmdClearColorImage(
		Task_CommandBuffer(task),
		final_image.image,
		VK_IMAGE_LAYOUT_GENERAL,
		&clear_color, 1, &range
	);

	Task_ExecutionBarrier(task, VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);

	vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, add_images.pipeline);

	const AddImages::PushConstantRange pc = {
		.image_width = Surface_Extent().width,
		.image_height = Surface_Extent().height,
		.image_count = static_cast<uint32_t>(color_targets.size()),
	};

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
		VkDeviceSize offset = 0;
		for (const Image& image : color_targets)
		{
			memcpy(
				reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset),
				image.descriptor.data(),
				image.descriptor.size()
			);
			offset += image.descriptor.size();
		}
		memcpy(
			reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset),
			gui_target.descriptor.data(),
			gui_target.descriptor.size()
		);
		offset += gui_target.descriptor.size();
		memcpy(
			reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset),
			final_image.descriptor.data(),
			final_image.descriptor.size()
		);

		const VkBindHeapInfoEXT bind_info = {
			.sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
			.heapRange = {
				.address = add_images.desc_heap.buffer.device_address,
				.size = add_images.desc_heap.buffer.allocation_info.allocationInfo.size,
			},
			.reservedRangeSize = max(add_images.desc_heap.buffer.allocation_info.allocationInfo.size, PhysicalDeviceData_DescHeapProps().minResourceHeapReservedRange)
		};

		const VkPushDataInfoEXT push_data = {
			.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
			.data = {
				.address = &pc,
				.size = sizeof(AddImages::PushConstantRange),
			}
		};

		vkCmdBindResourceHeapEXT(cmd_buff, &bind_info);
		vkCmdPushDataEXT(cmd_buff, &push_data);
	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
		memcpy(
			reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_buffer.buffer.allocation_info.allocationInfo.pMappedData) + add_images.desc_buffer.desc_binding_offsets[0]),
			final_image.descriptor.data(),
			final_image.descriptor.size()
		);

		memcpy(
			reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_buffer.buffer.allocation_info.allocationInfo.pMappedData) + add_images.desc_buffer.desc_binding_offsets[1]),
			gui_target.descriptor.data(),
			gui_target.descriptor.size()
		);

		VkDeviceSize color_target_offset = add_images.desc_buffer.desc_binding_offsets[2];

		for (const auto& color_target : color_targets)
		{
			memcpy(
				reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_buffer.buffer.allocation_info.allocationInfo.pMappedData) + color_target_offset),
				color_target.descriptor.data(),
				color_target.descriptor.size()
			);

			color_target_offset += color_target.descriptor.size();
		}

		const VkDescriptorBufferBindingInfoEXT binding_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
			.address = add_images.desc_buffer.buffer.device_address,
			.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT,
		};
		vkCmdBindDescriptorBuffersEXT(cmd_buff, 1, &binding_info);

		uint32_t buffers[1] = { 0 };
		VkDeviceSize offsets[1] = { 0 };

		vkCmdSetDescriptorBufferOffsetsEXT(
			cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, add_images.desc_buffer.pipeline_layout,
			0, 1,
			buffers, offsets
		);

		const VkPushConstantsInfo pc_info = {
			.sType = VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO,
			.layout = add_images.desc_buffer.pipeline_layout,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.size = sizeof(AddImages::PushConstantRange),
			.pValues = &pc
		};
		vkCmdPushConstants2(cmd_buff, &pc_info);
	}

	vkCmdDispatch(cmd_buff, Surface_Extent().width / 32 + 1, Surface_Extent().height / 32 + 1, 1);
}

void Task_BlitImageToSwapchain(const Task& task, const VkImage src_image, const VkImageLayout src_image_layout)
{
	const VkAcquireNextImageInfoKHR acq_info = {
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = Swapchain_Swapchain(),
		.timeout = UINT64_MAX,
		.semaphore = Swapchain_AcqSigSem(),
		.deviceMask = 1,
	};

	VkResult result = vkAcquireNextImage2KHR(Device_Device(), &acq_info, &swapchain_image_index);

	Task_ChangeImageLayout(
		task,
		src_image,
		src_image_layout, src_image_layout,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_READ_BIT
	);

	Task_ChangeImageLayout(
		task, Swapchain_Image(swapchain_image_index),
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR, 0,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT
	);

	const VkImageBlit2 region = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
		.srcSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
		},
		.srcOffsets = {{}, {.x = static_cast<int32_t>(Surface_Extent().width), .y = static_cast<int32_t>(Surface_Extent().height), .z = 1}},
		.dstSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
		},
		.dstOffsets = {{}, {.x = static_cast<int32_t>(Surface_Extent().width), .y = static_cast<int32_t>(Surface_Extent().height), .z = 1}},
	};

	VkBlitImageInfo2 blit_image_info = {
		.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.srcImage = src_image,
		.srcImageLayout = src_image_layout,
		.dstImage = Swapchain_Image(swapchain_image_index),
		.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.regionCount = 1,
		.pRegions = &region,
	};

	vkCmdBlitImage2(Task_CommandBuffer(task), &blit_image_info);
}

void Task_ChangeImageLayout(const Task& task, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, VkPipelineStageFlags2 src_stage_mask, VkAccessFlags2 src_access_mask, VkPipelineStageFlags2 dst_stage_mask, VkAccessFlags2 dst_access_mask)
{
	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	if (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
	{
		aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

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
			.aspectMask = aspect,
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

	vkCmdPipelineBarrier2(Task_CommandBuffer(task), &dependency_info);
}

void Task_End(const Task& task)
{
	Task_ChangeImageLayout(
		task, Swapchain_Image(swapchain_image_index),
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_BLIT_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0
	);

	VK_CHECK("end task", vkEndCommandBuffer(Task_CommandBuffer(task)));
}

void Task_Submit(Task& task, std::vector<VkSemaphoreSubmitInfo> wait_sem_infos, std::vector<VkSemaphoreSubmitInfo> sig_sem_infos)
{
	wait_sem_infos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Task_Semaphore(task),
			.value = Task_SemaphoreValue(task),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		}
		);

	wait_sem_infos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Swapchain_AcqSigSem(),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		}
		);

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
			.stageMask = VK_PIPELINE_STAGE_2_BLIT_BIT,
		}
		);
	sig_sem_infos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Swapchain_PrsntWaitSem(),
			.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		}
		);

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

void Task_Present(Task& task)
{
	VkSemaphore semaphore = Task_Semaphore(task);
	uint64_t semaphore_value = Task_SemaphoreValue(task);

	const VkSemaphoreWaitInfo wait_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.semaphoreCount = 1,
		.pSemaphores = &semaphore,
		.pValues = &semaphore_value,
	};

	VK_CHECK("wait semaphores", vkWaitSemaphores(Device_Device(), &wait_info, UINT64_MAX));

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

	VK_CHECK("queue present", vkQueuePresentKHR(task.queue, &present_info));
}

void Task_NextFrame(Task& task)
{
	task.current_frame = (task.current_frame + 1) % task.max_frames_in_flight;
}

void Task_BasicShapesNode_Destroy()
{
	const VkDevice device = Device_Device();
	
	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
		Allocator_DestroyBuffer(basic_shapes_node.desc_buffer.buffer);
		Allocator_DestroyBuffer(basic_shapes_node.mesh_data_buffer);

		vkDestroyPipelineLayout(device, basic_shapes_node.desc_buffer.pipeline_layout, nullptr);
		vkDestroyPipeline(device, basic_shapes_node.pipeline, nullptr);
	}
}

void Task_AddImages_Destroy()
{
	const VkDevice device = Device_Device();

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
		Allocator_DestroyBuffer(add_images.desc_heap.buffer);
	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
		Allocator_DestroyBuffer(add_images.desc_heap.buffer);

		vkDestroyDescriptorSetLayout(device, add_images.desc_buffer.desc_set_layout, nullptr);
		vkDestroyPipelineLayout(device, add_images.desc_buffer.pipeline_layout, nullptr);
	}

	vkDestroyPipeline(device, add_images.pipeline, nullptr);
}

void Task_Destroy(const Task& task)
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

