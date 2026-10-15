#include "utils.hpp"

void Utils_SetObjectName(VkDevice device, VkObjectType type, uint64_t handle, const std::string& name)
{
	const VkDebugUtilsObjectNameInfoEXT name_info = {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.objectType = type,
		.objectHandle = handle,
		.pObjectName = name.c_str(),
	};

	VK_CHECK("setting name", vkSetDebugUtilsObjectNameEXT(device, &name_info));
}
