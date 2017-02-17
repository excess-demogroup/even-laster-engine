#ifndef VULKAN_H
#define VULKAN_H

#define VK_PROTOTYPES

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <vector>
#include <cassert>

namespace vulkan
{
	extern VkInstance instance;
	extern VkDevice device;
	extern VkPhysicalDevice physicalDevice;
	extern VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	extern VkQueue graphicsQueue;
	extern int graphicsQueueIndex;

	extern VkCommandPool setupCommandPool;

	extern VkDebugReportCallbackEXT debugReportCallback;

	VkResult init(const char *appName, const std::vector<const char *> &enabledExtensions);

	extern struct instance_funcs {
		PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
		PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
	} instanceFuncs;

	static uint32_t getMemoryTypeIndex(const VkMemoryRequirements &memoryRequirements, VkMemoryPropertyFlags propertyFlags)
	{
		for (auto i = 0u; i < VK_MAX_MEMORY_TYPES; i++) {
			if (((memoryRequirements.memoryTypeBits >> i) & 1) == 1) {
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
					return i;
					break;
				}
			}
		}

		assert(false);
		throw std::runtime_error("invalid memory type!");
	}

	void instanceFuncsInit(VkInstance instance);
};

#endif // VULKAN_H
