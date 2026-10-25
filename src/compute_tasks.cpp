#include "compute_tasks.hpp"
#include "graphics_tasks.hpp"
#include "transfer_tasks.hpp"
#include "tasks.hpp"

#include "utils.hpp"
#include "vulkan_objects.hpp"

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
	//struct DescBuffer {
	//	Buffer buffer;
	//	VkPipelineLayout pipeline_layout;
	//	VkDescriptorSetLayout desc_set_layout;
	//	VkDeviceSize desc_set_layout_size;
	//};

	//// Indexing
	//struct DescIndexing {
	//	VkDescriptorPool desc_pool;
	//	VkDescriptorSet desc_set;
	//	VkPipelineLayout pipeline_layout;
	//	VkDescriptorSetLayout desc_set_layout;
	//};

	union {
		DescHeap desc_heap;
		//DescBuffer desc_buffer;
		//DescIndexing desc_indexing;
	};
};
struct CopyImageToSwapchain {
	struct PushConstantRange {
		uint32_t image_width;
		uint32_t image_height;
	};

	VkPipeline pipeline;

	// Heap
	struct DescHeap {
		Buffer buffer;
	};

	// Buffer
	//struct DescBuffer {
	//	Buffer buffer;
	//	VkPipelineLayout pipeline_layout;
	//	VkDescriptorSetLayout desc_set_layout;
	//	VkDeviceSize desc_set_layout_size;
	//};

	//// Indexing
	//struct DescIndexing {
	//	VkDescriptorPool desc_pool;
	//	VkDescriptorSet desc_set;
	//	VkPipelineLayout pipeline_layout;
	//	VkDescriptorSetLayout desc_set_layout;
	//};

	union {
		DescHeap desc_heap;
		//DescBuffer desc_buffer;
		//DescIndexing desc_indexing;
	};
};

static AddImages add_images = {};
static CopyImageToSwapchain copy_image_to_swapchain = {};

static std::vector<Image> _color_targets;
static Image _gui_target;

static Task compute_task = {};

void ComputeTask_AddImages_Create(const std::vector<Image>& color_targets, const Image& gui_target, const Image& final_image)
{
	VkShaderModule shader_module = VK_NULL_HANDLE;

	std::string shader_path = std::string(SDL_GetBasePath()).append("shaders\\glsl\\add_images.comp.glsl.spv");

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

	const VkPipelineShaderStageCreateInfo stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = shader_module,
		.pName = "main",
	};

	const VkPipelineCreateFlags2CreateInfo pipeline_create_flags = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
		.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
	};

	const VkComputePipelineCreateInfo pipeline_create_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = &pipeline_create_flags,
		.stage = stage_create_info,
	};

	VK_CHECK("create add images pipeline", vkCreateComputePipelines(Device_Device(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &add_images.pipeline));

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
		VkPhysicalDeviceDescriptorHeapPropertiesEXT desc_heap_props = PhysicalDeviceData_DescHeapProps();

		VkDeviceSize desc_size = ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment);
		VkDeviceSize total_heap_size = max(
			ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment) * (color_targets.size() + 1),
			desc_heap_props.minResourceHeapReservedRange
		);

		add_images.desc_heap.buffer = Allocator_CreateBuffer(
			total_heap_size, VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			"add images descriptor heap"
		);

		VkDeviceSize offset = 0;

		// color targets
		for (const auto& color_target : color_targets)
		{
			const VkImageViewCreateInfo iv_ci = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = color_target.image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.components = VK_COMPONENT_SWIZZLE_IDENTITY,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1,
				}
			};

			const VkImageDescriptorInfoEXT data = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
				.pView = &iv_ci,
				.layout = VK_IMAGE_LAYOUT_GENERAL,
			};

			const VkResourceDescriptorInfoEXT resources = {
				.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.data = &data,
			};

			void* addr = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset);
			VkHostAddressRangeEXT host_addr_rng = {
				.address = addr,
				.size = desc_size,
			};

			VK_CHECK("write resource descriptors", vkWriteResourceDescriptorsEXT(Device_Device(), 1, &resources, &host_addr_rng));
			offset += desc_size;
		}

		// gui target
		VkImageViewCreateInfo iv_ci = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = gui_target.image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.components = VK_COMPONENT_SWIZZLE_IDENTITY,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1,
			}
		};

		const VkImageDescriptorInfoEXT data = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
			.pView = &iv_ci,
			.layout = VK_IMAGE_LAYOUT_GENERAL,
		};

		const VkResourceDescriptorInfoEXT resources = {
			.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
			.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.data = &data,
		};
		void* addr = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset);
		VkHostAddressRangeEXT host_addr_rng = {
			.address = addr,
			.size = desc_size,
		};

		VK_CHECK("write resource descriptors", vkWriteResourceDescriptorsEXT(Device_Device(), 1, &resources, &host_addr_rng));
		offset += desc_size;

		// final image
		iv_ci.image = final_image.image;

		addr = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(add_images.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + offset);
		host_addr_rng.address = addr;
		host_addr_rng.size = desc_size;

		VK_CHECK("write resource descriptors", vkWriteResourceDescriptorsEXT(Device_Device(), 1, &resources, &host_addr_rng));
		offset += desc_size;
	}

	_color_targets = color_targets;
	_gui_target = gui_target;
	vkDestroyShaderModule(Device_Device(), shader_module, nullptr);
}

void ComputeTask_AddImages()
{
	const VkBindHeapInfoEXT bind_info = {
		.sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
		.heapRange = {
			.address = add_images.desc_heap.buffer.device_address,
			.size = add_images.desc_heap.buffer.allocation_info.allocationInfo.size,
		},
		.reservedRangeSize = add_images.desc_heap.buffer.allocation_info.allocationInfo.size,
	};

	const AddImages::PushConstantRange pc = {
		.image_width = Surface_Extent().width,
		.image_height = Surface_Extent().height,
		.image_count = static_cast<uint32_t>(_color_targets.size()) + 1 + 1,
	};

	const VkPushDataInfoEXT push_data = {
		.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
		.data = {
			.address = &pc,
			.size = sizeof(AddImages::PushConstantRange),
		}
	};

	VkCommandBuffer cmd_buff = Task_CommandBuffer(compute_task);

	vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, add_images.pipeline);
	vkCmdBindResourceHeapEXT(cmd_buff, &bind_info);
	vkCmdPushDataEXT(cmd_buff, &push_data);
	vkCmdDispatch(cmd_buff, Surface_Extent().width / 32 + 1, Surface_Extent().height / 32 + 1, 1);

	const VkMemoryBarrier2 mem_barr = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
	};

	const VkDependencyInfo dep_info = {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &mem_barr,
	};

	vkCmdPipelineBarrier2(cmd_buff, &dep_info);
}

void ComputeTask_AddImages_Destroy()
{
	vkDestroyPipeline(Device_Device(), add_images.pipeline, nullptr);
	Allocator_DestroyBuffer(add_images.desc_heap.buffer);
}

void ComputeTask_CopyImageToSwapchain_Create()
{
	VkShaderModule shader_module = VK_NULL_HANDLE;

	std::string shader_path = std::string(SDL_GetBasePath()).append("shaders\\glsl\\copy_image_to_swapchain.comp.glsl.spv");

	SDL_IOStream* io_stream = SDL_IOFromFile(shader_path.c_str(), "rb");
	size_t code_size = 0;
	uint32_t* code = reinterpret_cast<uint32_t*>(SDL_LoadFile_IO(io_stream, &code_size, true));

	const VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = static_cast<size_t>(code_size),
		.pCode = code,
	};

	VK_CHECK("create copy image shader module", vkCreateShaderModule(Device_Device(), &create_info, nullptr, &shader_module));
	SDL_free(code);

	const VkPipelineShaderStageCreateInfo stage_create_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = shader_module,
		.pName = "main",
	};

	const VkPipelineCreateFlags2CreateInfo pipeline_create_flags = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO,
		.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT,
	};

	const VkComputePipelineCreateInfo pipeline_create_info = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = &pipeline_create_flags,
		.stage = stage_create_info,
	};

	VK_CHECK("create copy image pipeline", vkCreateComputePipelines(Device_Device(), VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &copy_image_to_swapchain.pipeline));

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
		VkPhysicalDeviceDescriptorHeapPropertiesEXT desc_heap_props = PhysicalDeviceData_DescHeapProps();
		copy_image_to_swapchain.desc_heap.buffer = Allocator_CreateBuffer(
			max(2 * ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment), desc_heap_props.minResourceHeapReservedRange),
			VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			"copy image to swapchain descriptor heap"
		);
	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
	}
	else
	{
	}

	vkDestroyShaderModule(Device_Device(), shader_module, nullptr);
}

static uint32_t swapchain_image_index = 0;

uint32_t ComputeTask_SwapchainIndex()
{
	return swapchain_image_index;
}

void ComputeTask_CopyImageToSwapchain(const VkImage src_image)
{
	const VkAcquireNextImageInfoKHR acq_info = {
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = Swapchain_Swapchain(),
		.timeout = 0,
		.semaphore = Swapchain_AcqSigSem(),
		.deviceMask = 1,
	};

	VkResult result = vkAcquireNextImage2KHR(Device_Device(), &acq_info, &swapchain_image_index);

	while (result == VK_NOT_READY || result == VK_TIMEOUT)
	{
		result = vkAcquireNextImage2KHR(Device_Device(), &acq_info, &swapchain_image_index);
	}

	VkImage swapchain_image = Swapchain_Image(swapchain_image_index);

	if (descriptor_mode == EDescriptorMode::DescHeap)
	{
		VkPhysicalDeviceDescriptorHeapPropertiesEXT desc_heap_props = PhysicalDeviceData_DescHeapProps();
		copy_image_to_swapchain.desc_heap.buffer = Allocator_CreateBuffer(
			ALIGNED_SIZE(max(2 * ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment), desc_heap_props.minResourceHeapReservedRange), desc_heap_props.imageDescriptorAlignment),
			VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT,
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			"copy image to swapchain descriptor heap"
		);

		const VkImageViewCreateInfo final_image_view_create_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = src_image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			}
		};

		const VkImageDescriptorInfoEXT final_image_desc_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
			.pView = &final_image_view_create_info,
			.layout = VK_IMAGE_LAYOUT_GENERAL,
		};

		const VkResourceDescriptorDataEXT final_image_data = {
			.pImage = &final_image_desc_info,
		};

		const VkImageViewCreateInfo swapchain_image_view_create_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1,
			}
		};
		const VkImageDescriptorInfoEXT swapchain_image_desc_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT,
			.pView = &swapchain_image_view_create_info,
			.layout = VK_IMAGE_LAYOUT_GENERAL,
		};

		const VkResourceDescriptorDataEXT swapchain_image_data = {
			.pImage = &swapchain_image_desc_info,
		};

		const VkResourceDescriptorInfoEXT resources[] = {
			{
				.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.data = final_image_data,
			},
			{
				.sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT,
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.data = swapchain_image_data,
			}
		};

		const VkHostAddressRangeEXT addr_ranges[] = {
			{
				.address = copy_image_to_swapchain.desc_heap.buffer.allocation_info.allocationInfo.pMappedData,
				.size = ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment),
			},
			{
				.address = reinterpret_cast<void*>(reinterpret_cast<uint64_t>(copy_image_to_swapchain.desc_heap.buffer.allocation_info.allocationInfo.pMappedData) + ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment)),
				.size = ALIGNED_SIZE(desc_heap_props.imageDescriptorSize, desc_heap_props.imageDescriptorAlignment),
			}
		};

		VK_CHECK("write resource desc", vkWriteResourceDescriptorsEXT(Device_Device(), _countof(resources), resources, addr_ranges));
	}
	else if (descriptor_mode == EDescriptorMode::DescBuffer)
	{
	}
	else
	{
	}

	const VkBindHeapInfoEXT bind_info = {
		.sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
		.heapRange = {
			.address = copy_image_to_swapchain.desc_heap.buffer.device_address,
			.size = copy_image_to_swapchain.desc_heap.buffer.allocation_info.allocationInfo.size,
		},
		.reservedRangeSize = copy_image_to_swapchain.desc_heap.buffer.allocation_info.allocationInfo.size,
	};

	const CopyImageToSwapchain::PushConstantRange pc = {
		.image_width = Surface_Extent().width,
		.image_height = Surface_Extent().height,
	};

	const VkPushDataInfoEXT push_data = {
		.sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
		.data = {
			.address = &pc,
			.size = sizeof(CopyImageToSwapchain::PushConstantRange),
		}
	};

	VkCommandBuffer cmd_buff = Task_CommandBuffer(compute_task);
	Task_ChangeImageLayout(
		compute_task, swapchain_image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT
	);

	vkCmdBindPipeline(cmd_buff, VK_PIPELINE_BIND_POINT_COMPUTE, copy_image_to_swapchain.pipeline);
	vkCmdBindResourceHeapEXT(cmd_buff, &bind_info);
	vkCmdPushDataEXT(cmd_buff, &push_data);
	vkCmdDispatch(cmd_buff, Surface_Extent().width / 32 + 1, Surface_Extent().height / 32 + 1, 1);

	Task_ChangeImageLayout(
		compute_task, swapchain_image,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0
	);
}

void ComputeTask_CopyImageToSwapchain_Destroy()
{
	vkDestroyPipeline(Device_Device(), copy_image_to_swapchain.pipeline, nullptr);
	Allocator_DestroyBuffer(copy_image_to_swapchain.desc_heap.buffer);
}

void ComputeTask_Create(uint32_t max_frames_in_flight)
{
	Task_Create(compute_task, Device_Compute_Queue(), PhysicalDeviceData_ComputeQueueFamilyIndex(), max_frames_in_flight, "compute task");
}

void ComputeTask_Begin()
{
	Task_Begin(compute_task);
}

void ComputeTask_End()
{
	Task_End(compute_task);
}

void ComputeTask_Submit()
{
	const std::vector<VkSemaphoreSubmitInfo> wait_sem_infos = {
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = GraphicsTask_Semaphore(),
			.value = GraphicsTask_SemaphoreValue(),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		},
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = ComputeTask_Semaphore(),
			.value = ComputeTask_SemaphoreValue(),
			.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		},
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Swapchain_AcqSigSem(),
			.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		}
	};

	const std::vector<VkSemaphoreSubmitInfo> sig_sem_infos = {
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = ComputeTask_Semaphore(),
			.value = ComputeTask_SemaphoreValue() + 1,
			.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		},
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Swapchain_PrsntWaitSem(),
			.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		}
	};

	//printf("compute submit\n");
	//printf("graphics sem value: %lld compute sem value: %lld\n", wait_sem_infos.at(0).value, wait_sem_infos.at(1).value);
	Task_Submit(compute_task, wait_sem_infos, sig_sem_infos);
}

void ComputeTask_NextFrame()
{
	Task_NextFrame(compute_task);
	Swapchain_NextFrame();
}

void ComputeTask_Destroy()
{
	Task_Destroy(compute_task);
	ComputeTask_CopyImageToSwapchain_Destroy();
	ComputeTask_AddImages_Destroy();
}

VkSemaphore ComputeTask_Semaphore()
{
	return Task_Semaphore(compute_task);
}

uint64_t ComputeTask_SemaphoreValue()
{
	return Task_SemaphoreValue(compute_task);
}
