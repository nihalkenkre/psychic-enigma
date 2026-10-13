#include "vulkan_objects.hpp"
#include <vector>
#include <SDL3\SDL_vulkan.h>

#include "utils.hpp"

static VkInstance instance = VK_NULL_HANDLE;
static uint32_t vk_api_version;
static VkSurfaceKHR surface = VK_NULL_HANDLE;

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
		.pApplicationName = "Ten Minute Physics",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "Ten Minute Physics",
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

void Instance_Destroy()
{
	if (instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(instance, nullptr);
	}
}

void Surface_Create(SDL_Window* window)
{
	SDL_CHECK(SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface));
}

void Surface_Destroy()
{
	SDL_Vulkan_DestroySurface(instance, surface, nullptr);
}
