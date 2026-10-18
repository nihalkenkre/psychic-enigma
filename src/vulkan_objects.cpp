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

	std::vector<VkSemaphore> acq_sig_sems;
	std::vector<VkSemaphore> prsnt_wait_sems;

	uint32_t max_frames_in_flight;
	uint32_t current_frame_in_flight;
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

VkInstance Instance_Instance()
{
	return instance;
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

VkPhysicalDevice PhysicalDeviceData_PhysicalDevice()
{
	return physical_device_data.physical_device;
}

uint32_t PhysicalDeviceData_GraphicsQueueFamilyIndex()
{
	return physical_device_data.graphics_queue_family_index;
}

uint32_t PhysicalDeviceData_ComputeQueueFamilyIndex()
{
	return physical_device_data.compute_queue_family_index;
}

uint32_t PhysicalDeviceData_TransferQueueFamilyIndex()
{
	return physical_device_data.transfer_queue_family_index;
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
			{ return (frm.format == VK_FORMAT_R8G8B8A8_SRGB) && (frm.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR); });

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

VkExtent2D Surface_Extent()
{
	return surface_data.capabilities.surfaceCapabilities.currentExtent;
}

uint32_t Surface_MinImageCount()
{
	return surface_data.capabilities.surfaceCapabilities.minImageCount;
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

VkQueue Device_Graphics_Queue()
{
	return graphics_queue;
}

VkQueue Device_Compute_Queue()
{
	return compute_queue;
}

VkQueue Device_Transfer_Queue()
{
	return transfer_queue;
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
		.imageUsage = surface_data.capabilities.surfaceCapabilities.supportedUsageFlags & ~VK_IMAGE_USAGE_STORAGE_BIT,
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

VkSwapchainKHR Swapchain_Swapchain()
{
	return swapchain_data.swapchain;
}

VkSemaphore Swapchain_AcqSigSem()
{
	return swapchain_data.acq_sig_sems.at(swapchain_data.current_frame_in_flight);
}

VkSemaphore Swapchain_PrsntWaitSem()
{
	return swapchain_data.prsnt_wait_sems.at(swapchain_data.current_frame_in_flight);
}

VkImage Swapchain_Image(uint32_t image_index)
{
	return swapchain_data.images.at(image_index);
}

uint32_t Swapchain_MaxFramesInFlight()
{
	return swapchain_data.max_frames_in_flight;
}

uint32_t Swapchain_ImageCount()
{
	return swapchain_data.image_count;
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
	VmaAllocatorCreateInfo allocator_create_info = {
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = physical_device_data.physical_device,
		.device = device,
		.instance = instance,
		.vulkanApiVersion = vk_api_version,
	};

	VmaVulkanFunctions vk_funcs_ptr = {};
	vmaImportVulkanFunctionsFromVolk(&allocator_create_info, &vk_funcs_ptr);
	allocator_create_info.pVulkanFunctions = &vk_funcs_ptr;

	VK_CHECK("create allocator", vmaCreateAllocator(&allocator_create_info, &allocator));
}

void Allocator_Destroy()
{
	vmaDestroyAllocator(allocator);
}

Image Image_Create(const VkFormat format, const VkExtent3D& extent, const VkImageUsageFlags usage, const std::string& name)
{
	const uint32_t queue_family_indices[] = { physical_device_data.graphics_queue_family_index, physical_device_data.compute_queue_family_index, physical_device_data.transfer_queue_family_index };

	Image image = {};

	const VkImageCreateInfo image_create_info = {
    	.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
	.imageType = VK_IMAGE_TYPE_2D,
	.format = format,
	.extent = extent,
	.mipLevels = 1,
	.arrayLayers = 1,
	.samples = VK_SAMPLE_COUNT_1_BIT,
	.tiling = VK_IMAGE_TILING_OPTIMAL,
	.usage = usage,
	.queueFamilyIndexCount = _countof(queue_family_indices),
	.pQueueFamilyIndices = queue_family_indices,
	};
	const VmaAllocationCreateInfo allocation_create_info = { .usage = VMA_MEMORY_USAGE_AUTO };

	VK_CHECK("create image", vmaCreateImage(allocator, &image_create_info, &allocation_create_info, &image.image, &image.allocation, &image.allocation_info.allocationInfo));

	const VkImageViewCreateInfo image_view_create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = image.image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		},
	};

	VK_CHECK("create image view", vkCreateImageView(device, &image_view_create_info, nullptr, &image.image_view));

	const VkSamplerCreateInfo sampler_create_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
	};

	VK_CHECK("create sampler", vkCreateSampler(device, &sampler_create_info, nullptr, &image.sampler));

#ifdef _DEBUG
	Utils_SetObjectName(device, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(image.image), name + " image");
	Utils_SetObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(image.image_view), name + " image view");
	Utils_SetObjectName(device, VK_OBJECT_TYPE_SAMPLER, reinterpret_cast<uint64_t>(image.sampler), name + " sampler");
#endif

	return image;
}

void Image_Destroy(Image image)
{
	if (device != VK_NULL_HANDLE)
	{
		if (image.sampler != VK_NULL_HANDLE)
		{
			vkDestroySampler(device, image.sampler, nullptr);
		}

		if (image.image_view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, image.image_view, nullptr);
		}

		if (image.image != VK_NULL_HANDLE)
		{
			vmaDestroyImage(allocator, image.image, image.allocation);
		}
	}
}
