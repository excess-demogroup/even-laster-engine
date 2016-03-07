#include <stdafx.h>

static VkInstance instance;

VkResult instanceInit(const char *appName, bool enableValidation)
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.pEngineName = "very lastest engine ever";
	// Temporary workaround for drivers not supporting SDK 1.0.3 upon launch
	// todo : Use VK_API_VERSION 
	appInfo.apiVersion = VK_API_VERSION; // VK_MAKE_VERSION(1, 0, 2);

	const char *enabledExtensions[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions;
	instanceCreateInfo.enabledExtensionCount = ARRAY_SIZE(enabledExtensions) - 1;
	if (enableValidation)
		instanceCreateInfo.enabledExtensionCount++;

#if 0
	if (enableValidation) {
		instanceCreateInfo.enabledLayerCount = vkDebug::validationLayerCount; // todo : change validation layer names!
		instanceCreateInfo.ppEnabledLayerNames = vkDebug::validationLayerNames;
	}
#endif

	return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

