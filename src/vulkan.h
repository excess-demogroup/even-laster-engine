#ifndef VULKAN_H
#define VULKAN_H

#define VK_PROTOTYPES

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
#include <vector>

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

	void instanceFuncsInit(VkInstance instance);
};

#endif // VULKAN_H
