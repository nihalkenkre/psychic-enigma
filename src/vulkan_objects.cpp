#include "vulkan_objects.hpp"
#include <vector>
#include <SDL3\SDL_vulkan.h>
#include <vk_mem_alloc.h>

#include "utils.hpp"

struct PhysicalDeviceData {
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceProperties2KHR properties;
	VkPhysicalDeviceMemoryProperties2KHR memory_properties;
	uint32_t graphics_queue_family_index;
	uint32_t compute_queue_family_index;
	uint32_t transfer_queue_family_index;
};

struct SurfaceeData {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPresentModeKHR present_mode;
	VkSurfaceFormat2KHR surface_format;
	VkSurfaceCapabilities2KHR capabilities;
};

struct SwapchainData {
	VkSwapchainKHR swapchain;
	std::vector<VkImage> images;
	std::vector<VkImageView> image_views;
	uint32_t image_count;

	uint32_t max_frames_in_flight;
	uint32_t current_frame_in_flight;

	uint32_t acq_image_index;

	std::vector<VkSemaphore> acq_sig_sems;
	std::vector<VkSemaphore> prsnt_wait_sems;
};

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

static VkInstance instance = VK_NULL_HANDLE;
static uint32_t vk_api_version = 0;
static PhysicalDeviceData physical_device_data = {
.properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,},
.memory_properties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,}
};
static SurfaceeData surface_data = {
.capabilities = {
.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
} };

static VkDevice device = VK_NULL_HANDLE;
static VkQueue graphics_queue = VK_NULL_HANDLE;
static VkQueue compute_queue = VK_NULL_HANDLE;
static VkQueue transfer_queue = VK_NULL_HANDLE;

static SwapchainData swapchain_data = {};

static VmaAllocator allocator = VK_NULL_HANDLE;

static Task graphics_task = {};
static Task transfer_task = {};

constexpr uint32_t EXTRA_FRAMES_IN_FLIGHT = 2;

void Instance_Create()
{
	Uint32 extensions_count = 0;
	auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensions_count);

	std::vector<const char*> more_exts = {
		VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
#ifdef _DEBUG
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
	};

	vk_api_version = VK_MAKE_API_VERSION(0, 1, 3, 0);

	std::vector<const char*> all_extensions;

	for (uint32_t ec = 0; ec < extensions_count; ++ec)
	{
		all_extensions.push_back(extensions[ec]);
	}

	all_extensions.append_range(more_exts);

	uint32_t property_count = 0;
	VK_CHECK("enumerate instance extensions", vkEnumerateInstanceExtensionProperties(nullptr, &property_count, nullptr));

	std::vector<VkExtensionProperties> props(property_count);
	VK_CHECK("enumerate instance extensions", vkEnumerateInstanceExtensionProperties(nullptr, &property_count, props.data()));

	for (auto const& req_ext_name : all_extensions)
	{
		auto it = std::find_if(props.begin(), props.end(), [&](const VkExtensionProperties prop)
			{ return (std::strcmp(prop.extensionName, req_ext_name) == 0); });

		if (it == props.end())
		{
			printf("Extension %s is not supported by instance", req_ext_name);
		}
	}

	const VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Psychic-Enigma",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "Psychic-Enigma",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = vk_api_version,
	};

	const VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = static_cast<uint32_t>(all_extensions.size()),
		.ppEnabledExtensionNames = all_extensions.data(),
	};

	VK_CHECK("create instance", vkCreateInstance(&create_info, nullptr, &instance));

	volkLoadInstance(instance);
}

void Instance_GetPhysicalDeviceData()
{
	uint32_t physical_device_count = 0;
	VK_CHECK("enumerate physical devices", vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));

	std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
	VK_CHECK("enumerate physical devices", vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

	for (auto const& physical_device : physical_devices)
	{
		vkGetPhysicalDeviceProperties2(physical_device, &physical_device_data.properties);

		if (physical_device_data.properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			uint32_t q_fly_cnt = 0;
			vkGetPhysicalDeviceQueueFamilyProperties2(physical_device, &q_fly_cnt, nullptr);

			std::vector<VkQueueFamilyProperties2KHR> q_fly_props(q_fly_cnt, { .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
			vkGetPhysicalDeviceQueueFamilyProperties2(physical_device, &q_fly_cnt, q_fly_props.data());

			// find graphics queue
			for (uint32_t q = 0; q < q_fly_cnt; ++q)
			{
				VkBool32 is_supported = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, q, surface_data.surface, &is_supported);

				if (is_supported &&
					SDL_Vulkan_GetPresentationSupport(instance, physical_device, q) &&
					q_fly_props[q].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					physical_device_data.physical_device = physical_device;
					physical_device_data.graphics_queue_family_index = q;
					physical_device_data.transfer_queue_family_index = q;
					physical_device_data.compute_queue_family_index = q;

					vkGetPhysicalDeviceMemoryProperties2(physical_device, &physical_device_data.memory_properties);

					break;
				}
			}

			// Find transfer queue
			for (int32_t q = (int32_t)q_fly_cnt - 1; q >= 0; --q)
			{
				if ((q_fly_props.at(q).queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(q_fly_props.at(q).queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) && !(q_fly_props[q].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT))
				{
					physical_device_data.transfer_queue_family_index = q;
					break;
				}
			}


			if (physical_device_data.transfer_queue_family_index == VK_QUEUE_FAMILY_IGNORED)
			{
				for (int32_t q = (int32_t)q_fly_cnt - 1; q >= 0; --q)
				{
					if (q_fly_props.at(q).queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT)
					{
						physical_device_data.transfer_queue_family_index = q;
						break;
					}
				}
			}

			// Find compute queue
			for (int32_t q = (int32_t)q_fly_cnt - 1; q >= 0; --q)
			{
				if ((q_fly_props.at(q).queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(q_fly_props.at(q).queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT))
				{
					physical_device_data.compute_queue_family_index = q;
					break;
				}
			}

			if (physical_device_data.compute_queue_family_index == VK_QUEUE_FAMILY_IGNORED)
			{
				for (int32_t q = (int32_t)q_fly_cnt - 1; q >= 0; --q)
				{
					if (q_fly_props.at(q).queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT)
					{
						physical_device_data.compute_queue_family_index = q;
						break;
					}
				}
			}
		}
	}
}

void Instance_Destroy()
{
	if (instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(instance, nullptr);
	}
}

void Surface_Create(SDL_Window* window)
{
	SDL_CHECK(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface_data.surface));
}

void Surface_PopulateData()
{
	{
		uint32_t surf_forms_count = 0;
		VK_CHECK("get surface formats", vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_data.physical_device, surface_data.surface, &surf_forms_count, nullptr));

		std::vector<VkSurfaceFormatKHR> surf_forms(surf_forms_count);
		VK_CHECK("get surface formats", vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_data.physical_device, surface_data.surface, &surf_forms_count, surf_forms.data()));

		auto it = std::find_if(surf_forms.begin(), surf_forms.end(), [](const VkSurfaceFormatKHR frm)
			{ return (frm.format == VK_FORMAT_R8G8B8A8_UNORM) && (frm.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR); });

		if (it != surf_forms.end())
		{
			surface_data.surface_format.surfaceFormat = *it;
		}
	}

	{
		uint32_t present_modes_count = 0;
		VK_CHECK("get present modes", vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_data.physical_device, surface_data.surface, &present_modes_count, nullptr));

		std::vector<VkPresentModeKHR> present_modes(present_modes_count);
		VK_CHECK("get surface formats", vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_data.physical_device, surface_data.surface, &present_modes_count, present_modes.data()));

		auto it = std::find_if(present_modes.begin(), present_modes.end(), [](const VkPresentModeKHR mode)
			{ return mode == VK_PRESENT_MODE_MAILBOX_KHR; });

		if (it != present_modes.end())
		{
			surface_data.present_mode = *it;
		}
	}

	const VkPhysicalDeviceSurfaceInfo2KHR surface_info = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
		.surface = surface_data.surface,
	};

	VK_CHECK("get surface capabilitites", vkGetPhysicalDeviceSurfaceCapabilities2KHR(physical_device_data.physical_device, &surface_info, &surface_data.capabilities));
}

void Surface_Destroy()
{
	SDL_Vulkan_DestroySurface(instance, surface_data.surface, nullptr);
}

void Device_Create()
{
	std::vector<const char*> req_ext_names = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,
	};

	uint32_t property_count = 0;
	VK_CHECK("enumerate device extensions", vkEnumerateDeviceExtensionProperties(physical_device_data.physical_device, nullptr, &property_count, nullptr));

	std::vector<VkExtensionProperties> props(property_count);
	VK_CHECK("enumerate device extensions", vkEnumerateDeviceExtensionProperties(physical_device_data.physical_device, nullptr, &property_count, props.data()));

	for (auto const& req_ext_name : req_ext_names)
	{
		auto it = std::find_if(props.begin(), props.end(), [&](const VkExtensionProperties prop)
			{ return (std::strcmp(prop.extensionName, req_ext_name) == 0); });

		if (it == props.end())
		{
			printf("Extension %s is not supported by device\n", req_ext_name);
		}
	}

	VkDeviceQueueCreateInfo q_ci = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = physical_device_data.graphics_queue_family_index,
		.queueCount = 1,
	};

	std::vector<VkDeviceQueueCreateInfo> d_q_cis{ q_ci };
	auto d_q_ci_it = std::find_if(std::begin(d_q_cis), std::end(d_q_cis), [&](const VkDeviceQueueCreateInfo& d_q_ci) {return d_q_ci.queueFamilyIndex == physical_device_data.transfer_queue_family_index; });

	if (d_q_ci_it == std::end(d_q_cis))
	{
		q_ci.queueFamilyIndex = physical_device_data.transfer_queue_family_index;
		q_ci.queueCount = 1;
		d_q_cis.push_back(q_ci);
	}
	else
	{
		++d_q_ci_it->queueCount;
	}

	d_q_ci_it = std::find_if(std::begin(d_q_cis), std::end(d_q_cis), [&](const VkDeviceQueueCreateInfo& d_q_ci) {return d_q_ci.queueFamilyIndex == physical_device_data.compute_queue_family_index; });

	if (d_q_ci_it == std::end(d_q_cis))
	{
		q_ci.queueFamilyIndex = physical_device_data.compute_queue_family_index;
		q_ci.queueCount = 1;
		d_q_cis.push_back(q_ci);
	}
	else
	{
		++d_q_ci_it->queueCount;
	}

	std::vector<std::vector<float>> priorities(d_q_cis.size());
	size_t d_q_ci_idx = 0;
	for (auto& d_q_ci : d_q_cis)
	{
		priorities[d_q_ci_idx].resize(d_q_ci.queueCount, 1.f);
		d_q_ci.pQueuePriorities = priorities[d_q_ci_idx].data();
	}

	VkPhysicalDeviceDynamicRenderingFeatures dyn_rend_feats = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
	};

	VkPhysicalDeviceSynchronization2Features sync2_feats = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
		.pNext = &dyn_rend_feats,
	};

	VkPhysicalDeviceVulkan11Features feats11 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &sync2_feats,
	};

	VkPhysicalDeviceVulkan12Features feats12 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &feats11,
	};

	VkPhysicalDeviceFeatures2 feats2 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &feats12,
	};

	vkGetPhysicalDeviceFeatures2(physical_device_data.physical_device, &feats2);

	const VkDeviceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &feats2,
		.queueCreateInfoCount = static_cast<uint32_t>(d_q_cis.size()),
		.pQueueCreateInfos = d_q_cis.data(),
		.enabledExtensionCount = static_cast<uint32_t>(req_ext_names.size()),
		.ppEnabledExtensionNames = req_ext_names.data(),
	};

	VK_CHECK("create device", vkCreateDevice(physical_device_data.physical_device, &create_info, nullptr, &device));

	VkDeviceQueueInfo2 queue_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
	};

	for (const auto& d_q_ci : d_q_cis)
	{
		for (uint32_t q = 0; q < d_q_ci.queueCount; ++q)
		{
			if (d_q_ci.queueFamilyIndex == physical_device_data.graphics_queue_family_index && graphics_queue == VK_NULL_HANDLE)
			{
				queue_info.queueFamilyIndex = physical_device_data.graphics_queue_family_index;
				queue_info.queueIndex = q;
				vkGetDeviceQueue2(device, &queue_info, &graphics_queue);
			}
			else if (d_q_ci.queueFamilyIndex == physical_device_data.transfer_queue_family_index && transfer_queue == VK_NULL_HANDLE)
			{
				queue_info.queueFamilyIndex = physical_device_data.transfer_queue_family_index;
				queue_info.queueIndex = q;
				vkGetDeviceQueue2(device, &queue_info, &transfer_queue);
			}
			else if (d_q_ci.queueFamilyIndex == physical_device_data.compute_queue_family_index && compute_queue == VK_NULL_HANDLE)
			{
				queue_info.queueFamilyIndex = physical_device_data.compute_queue_family_index;
				queue_info.queueIndex = q;
				vkGetDeviceQueue2(device, &queue_info, &compute_queue);
			}
		}
	}

#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_DEVICE, reinterpret_cast<uint64_t>(device), "device");
	Utils_SetObjectName(device, VK_OBJECT_TYPE_PHYSICAL_DEVICE, reinterpret_cast<uint64_t>(physical_device_data.physical_device), physical_device_data.properties.properties.deviceName);
	Utils_SetObjectName(device, VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64_t>(graphics_queue), "graphics queue");
	Utils_SetObjectName(device, VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64_t>(transfer_queue), "transfer queue");
	Utils_SetObjectName(device, VK_OBJECT_TYPE_QUEUE, reinterpret_cast<uint64_t>(compute_queue), "compute queue");
#endif	// _DEBUG
}

VkDevice Device_Device()
{
	return device;
}

void Device_Destroy()
{
	if (device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(device, nullptr);
	}
}

void Swapchain_Create()
{
	const VkSwapchainCreateInfoKHR create_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface_data.surface,
		.minImageCount = surface_data.capabilities.surfaceCapabilities.minImageCount,
		.imageFormat = surface_data.surface_format.surfaceFormat.format,
		.imageColorSpace = surface_data.surface_format.surfaceFormat.colorSpace,
		.imageExtent = surface_data.capabilities.surfaceCapabilities.currentExtent,
		.imageArrayLayers = 1,
		.imageUsage = surface_data.capabilities.surfaceCapabilities.supportedUsageFlags,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &physical_device_data.graphics_queue_family_index,
		.preTransform = surface_data.capabilities.surfaceCapabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = surface_data.present_mode,
		.oldSwapchain = VK_NULL_HANDLE,
	};

	swapchain_data.max_frames_in_flight = create_info.minImageCount + EXTRA_FRAMES_IN_FLIGHT;

	VK_CHECK("create swapchain", vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain_data.swapchain));
	VK_CHECK("get swapchain images", vkGetSwapchainImagesKHR(device, swapchain_data.swapchain, &swapchain_data.image_count, nullptr));

	swapchain_data.images.resize(swapchain_data.image_count);
	VK_CHECK("get swapchain images", vkGetSwapchainImagesKHR(device, swapchain_data.swapchain, &swapchain_data.image_count, swapchain_data.images.data()));

	VkImageViewCreateInfo image_view_create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = surface_data.surface_format.surfaceFormat.format,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1,
		},
	};

	swapchain_data.image_views.resize(swapchain_data.image_count);
	for (uint32_t i = 0; i < swapchain_data.image_count; ++i)
	{
		image_view_create_info.image = swapchain_data.images.at(i);
		VK_CHECK("create swapchain image view", vkCreateImageView(device, &image_view_create_info, nullptr, &swapchain_data.image_views.at(i)));
	}

	const VkSemaphoreCreateInfo sem_ci = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	swapchain_data.acq_sig_sems.resize(swapchain_data.max_frames_in_flight);
	swapchain_data.prsnt_wait_sems.resize(swapchain_data.max_frames_in_flight);

	for (uint32_t i = 0; i < swapchain_data.max_frames_in_flight; ++i)
	{
		VK_CHECK("create acq sig sem", vkCreateSemaphore(device, &sem_ci, nullptr, swapchain_data.acq_sig_sems.data() + i));
		VK_CHECK("create prsnt wait sem", vkCreateSemaphore(device, &sem_ci, nullptr, swapchain_data.prsnt_wait_sems.data() + i));
	}

#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<uint64_t>(swapchain_data.swapchain), "swapchain");

	for (uint32_t i = 0; i < swapchain_data.image_count; ++i)
	{
		Utils_SetObjectName(device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(swapchain_data.images[i]), std::string("swapchain image ").append(std::to_string(i)).c_str());
		Utils_SetObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(swapchain_data.image_views[i]), std::string("swapchain image view ").append(std::to_string(i)).c_str());
	}

	for (uint32_t i = 0; i < swapchain_data.max_frames_in_flight; ++i)
	{
		Utils_SetObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(swapchain_data.acq_sig_sems[i]), std::string("swapchain acq sig sem ").append(std::to_string(i)).c_str());
		Utils_SetObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(swapchain_data.prsnt_wait_sems[i]), std::string("swapchain prsnt sig sem ").append(std::to_string(i)).c_str());
	}
#endif
}

VkSemaphore Swapchain_AcqSigSem()
{
	return swapchain_data.acq_sig_sems.at(swapchain_data.current_frame_in_flight);
}

VkSemaphore Swapchain_PrsntWaitSem()
{
	return swapchain_data.prsnt_wait_sems.at(swapchain_data.current_frame_in_flight);
}

void Swapchain_NextFrame()
{
	swapchain_data.current_frame_in_flight = (swapchain_data.current_frame_in_flight + 1) % swapchain_data.max_frames_in_flight;
}

void Swapchain_Destroy()
{
	if (device != VK_NULL_HANDLE)
	{
		for (auto const image_view : swapchain_data.image_views)
		{
			vkDestroyImageView(device, image_view, nullptr);
		}

		for (uint32_t i = 0; i < swapchain_data.max_frames_in_flight; ++i)
		{
			vkDestroySemaphore(device, swapchain_data.acq_sig_sems[i], nullptr);
			vkDestroySemaphore(device, swapchain_data.prsnt_wait_sems[i], nullptr);
		}

		if (swapchain_data.swapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device, swapchain_data.swapchain, nullptr);
		}
	}
}

void Allocator_Create()
{
	const VmaAllocatorCreateInfo allocator_create_info = {
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = physical_device_data.physical_device,
		.device = device,
		.instance = instance,
		.vulkanApiVersion = vk_api_version,
	};

	VK_CHECK("create allocator", vmaCreateAllocator(&allocator_create_info, &allocator));

}

void Allocator_Destroy()
{
	vmaDestroyAllocator(allocator);
}

void Task_Create(Task& task, VkQueue queue, const uint32_t queue_family_index, const std::string& name)
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
		.queueFamilyIndex = physical_device_data.graphics_queue_family_index,
	};

	VK_CHECK("create command pool", vkCreateCommandPool(device, &cmd_pool_ci, nullptr, &task.command_pool));

#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<uint64_t>(task.command_pool), name + " command pool");
#endif

	task.max_frames_in_flight = swapchain_data.max_frames_in_flight;
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

	VK_CHECK("wait semaphores", vkWaitSemaphores(device, &wait_info, UINT64_MAX));

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

void Task_Submit(Task& task)
{
	const VkSemaphoreSubmitInfo wait_sem_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Task_Semaphore(task),
			.value = Task_SemaphoreValue(task),
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
},
{
	.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
	.semaphore = Swapchain_AcqSigSem(),
	.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
}
	};

	const VkCommandBufferSubmitInfo cmd_buff_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = Task_CommandBuffer(task),
}
	};

	const VkSemaphoreSubmitInfo sig_sem_infos[] = {
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = Task_Semaphore(task),
			.value = ++Task_SemaphoreValue(task),
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
},
{
	.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
	.semaphore = Swapchain_PrsntWaitSem(),
	.stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
}
	};

	const VkSubmitInfo2 submit_infos[] = {
		{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount = _countof(wait_sem_infos),
		.pWaitSemaphoreInfos = wait_sem_infos,
		.commandBufferInfoCount = _countof(cmd_buff_infos),
		.pCommandBufferInfos = cmd_buff_infos,
		.signalSemaphoreInfoCount = _countof(sig_sem_infos),
		.pSignalSemaphoreInfos = sig_sem_infos,
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
	if (device != VK_NULL_HANDLE)
	{
		if (task.command_pool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(device, task.command_pool, nullptr);
		}

		for (const auto sem : task.semaphores)
		{
			vkDestroySemaphore(device, sem, nullptr);
		}
	}
}

void GraphicsTask_Create()
{
	Task_Create(graphics_task, graphics_queue, physical_device_data.graphics_queue_family_index, "graphics task");
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

// Render to a different color target,
// then blit the color target to the swapchain.
void GraphicsTask_BeginRenderPass()
{
	const VkAcquireNextImageInfoKHR acq_info = {
		.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
		.swapchain = swapchain_data.swapchain,
		.timeout = UINT64_MAX,
		.semaphore = Swapchain_AcqSigSem(),
		.deviceMask = 1,
	};

	VK_CHECK("acquire next image", vkAcquireNextImage2KHR(device, &acq_info, &swapchain_data.acq_image_index));

	GraphicsTask_ChangeImageLayout(
		swapchain_data.images.at(swapchain_data.acq_image_index),
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_ACCESS_2_NONE,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
	);

	const VkClearValue clear_value = {
		.color = {
			.float32 = {(float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, 1},
		}
	};

	const VkRenderingAttachmentInfo color_attachments[] = {
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = swapchain_data.image_views.at(swapchain_data.acq_image_index),
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clear_value,
		},
	};
	const VkRenderingInfo rendering_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = VkRect2D{.extent = surface_data.capabilities.surfaceCapabilities.currentExtent },
		.layerCount = 1,
		.colorAttachmentCount = _countof(color_attachments),
		.pColorAttachments = color_attachments,
	};

	vkCmdBeginRendering(Task_CommandBuffer(graphics_task), &rendering_info);
}

void GraphicsTask_EndRenderPass()
{
	vkCmdEndRendering(Task_CommandBuffer(graphics_task));
	GraphicsTask_ChangeImageLayout(
		swapchain_data.images.at(swapchain_data.acq_image_index),
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, VK_ACCESS_2_NONE
	);
}

void GraphicsTask_End()
{
	Task_End(graphics_task);
}

void GraphicsTask_Submit()
{
	Task_Submit(graphics_task);
}

void GraphicsTask_Present()
{
	VkSemaphore wait_sem = Swapchain_PrsntWaitSem();

	const VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &wait_sem,
		.swapchainCount = 1,
		.pSwapchains = &swapchain_data.swapchain,
		.pImageIndices = &swapchain_data.acq_image_index,
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

void TransferTask_Create()
{
	Task_Create(transfer_task, transfer_queue, physical_device_data.transfer_queue_family_index, "transfer task");
}

void TransferTask_Begin()
{
	Task_Start(transfer_task);
}

void TransferTask_End()
{
	Task_End(transfer_task);
}

void TransferTask_Submit()
{
	Task_Submit(transfer_task);
}

void TransferTask_NextFrame()
{
	Task_NextFrame(transfer_task);
}

void TransferTask_Destroy()
{
	Task_Destroy(transfer_task);
}

Image Image_Create()
{
	Image image = {};
	const VkImageCreateInfo create_info = {};
	const VmaAllocationCreateInfo allocation_create_info = {};

	VK_CHECK("create image", vmaCreateImage(allocator, &create_info, &allocation_create_info, &image.image, &image.allocation, &image.allocation_info.allocationInfo));

	return image;
}

void Image_Destroy(Image image)
{
	if (device != VK_NULL_HANDLE)
	{
		if (image.image != VK_NULL_HANDLE)
		{
			vmaDestroyImage(allocator, image.image, image.allocation);
		}

		if (image.image_view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, image.image_view, nullptr);
		}
	}
}
