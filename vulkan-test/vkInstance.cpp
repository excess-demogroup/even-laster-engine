#include "stdafx.h"

#include "vulkan.h"
#include <stdio.h>

using namespace vulkan;
VkInstance vulkan::instance;
VkDevice vulkan::device;
VkPhysicalDevice vulkan::physicalDevice;
VkPhysicalDeviceMemoryProperties vulkan::deviceMemoryProperties;
VkQueue vulkan::graphicsQueue;
VkDebugReportCallbackEXT vulkan::debugReportCallback;

static const char *validationLayerNames[] = {
	"VK_LAYER_GOOGLE_unique_objects",
	"VK_LAYER_LUNARG_device_limits",
	"VK_LAYER_LUNARG_draw_state",
	"VK_LAYER_LUNARG_image",
	"VK_LAYER_LUNARG_mem_tracker",
	"VK_LAYER_LUNARG_object_tracker",
	"VK_LAYER_LUNARG_param_checker",
	"VK_LAYER_LUNARG_swapchain",
	"VK_LAYER_LUNARG_threading",
};

void instanceInit(const char *appName)
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName;
	appInfo.pEngineName = "very lastest engine ever";
	appInfo.apiVersion = VK_API_VERSION;

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

#ifndef NDEBUG
	instanceCreateInfo.enabledExtensionCount++;
	instanceCreateInfo.ppEnabledLayerNames = validationLayerNames;
	instanceCreateInfo.enabledLayerCount = ARRAY_SIZE(validationLayerNames);
#endif

	VkResult err = vkCreateInstance(&instanceCreateInfo, nullptr, &vulkan::instance);
	assert(err == VK_SUCCESS);
}

static VkBool32 messageCallback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t srcObject,
	size_t location,
	int32_t msgCode,
	const char *pLayerPrefix,
	const char *pMsg,
	void *pUserData)
{
	size_t message_len = strlen(pMsg) + 1000;
	char *message = new char[message_len];
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		snprintf(message, message_len, "ERROR: [%s] Code %d : %s\n", pLayerPrefix, msgCode, pMsg);
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		snprintf(message, message_len, "WARNING: [%s] Code %d : %s\n", pLayerPrefix, msgCode, pMsg);
	else {
		delete[] message;
		return false;
	}

	OutputDebugStringA(message);
	delete[] message;
	return false;
}

VkResult vulkan::init(const char *appName)
{
	instanceInit(appName);
	instanceFuncsInit(instance);

	VkResult err;

#ifndef NDEBUG
	VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = { };
	debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	debugReportCallbackCreateInfo.pNext = NULL;
	debugReportCallbackCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)messageCallback;
	debugReportCallbackCreateInfo.pUserData = NULL;
	debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	err = instanceFuncs.vkCreateDebugReportCallbackEXT(instance, &debugReportCallbackCreateInfo,
	                                                   nullptr, &debugReportCallback);
	assert(err == VK_SUCCESS);

	// SELF-TEST:
	// instanceFuncs.vkDebugReportMessageEXT(instance, VK_DEBUG_REPORT_WARNING_BIT_EXT, VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT, NULL, 0, 0, "self-test", "This is a dummy warning");
#endif

	// Get number of available physical devices
	uint32_t physDevCount = 0;
	err = vkEnumeratePhysicalDevices(instance, &physDevCount, nullptr);
	assert(err == VK_SUCCESS);
	assert(physDevCount > 0);

	// Enumerate devices
	VkPhysicalDevice *physicalDevices = new VkPhysicalDevice[physDevCount];
	err = vkEnumeratePhysicalDevices(instance, &physDevCount, physicalDevices);
	assert(err == VK_SUCCESS);
	for (uint32_t i = 0; i < physDevCount; ++i) {

		VkPhysicalDeviceProperties deviceProps;
		vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);

		VkPhysicalDeviceFeatures deviceFeats;
		vkGetPhysicalDeviceFeatures(physicalDevices[i], &deviceFeats);

		uint32_t propCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &propCount, NULL);
		assert(propCount > 0);

		VkQueueFamilyProperties *props = new VkQueueFamilyProperties[propCount];
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &propCount, props);

		OutputDebugString(deviceProps.deviceName);
	}

	// just pick the first one for now!
	physicalDevice = physicalDevices[0];

	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
	assert(queueCount > 0);

	VkQueueFamilyProperties *props = new VkQueueFamilyProperties[queueCount];
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, props);
	int graphicsQueueIndex = -1;
	for (size_t i = 0; i < queueCount; i++) {
		if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphicsQueueIndex = (int)i;
			break;
		}
	}
	assert(graphicsQueueIndex >= 0);

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	float queuePriorities = 0.0f;
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriorities;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = NULL;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.pEnabledFeatures = NULL;

	const char *enabledExtensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,

/*		VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME */
	};
	deviceCreateInfo.enabledExtensionCount = ARRAY_SIZE(enabledExtensions);
	deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions;

#ifndef NDEBUG
//	deviceCreateInfo.enabledExtensionCount++;
	deviceCreateInfo.ppEnabledLayerNames = validationLayerNames;
	deviceCreateInfo.enabledLayerCount = ARRAY_SIZE(validationLayerNames);
#endif


	err = vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device);
	assert(err == VK_SUCCESS);
	deviceFuncsInit(device);

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
	vkGetDeviceQueue(device, graphicsQueueIndex, 0, &graphicsQueue);

	return err;
}

template <typename T>
static T getDeviceProc(VkDevice device, const char *entrypoint)
{
	void *ret = vkGetDeviceProcAddr(device, entrypoint);
	assert(ret != nullptr);
	return (T)ret;
}

void vulkan::deviceFuncsInit(VkDevice device)
{
	deviceFuncs.vkQueuePresentKHR = getDeviceProc<PFN_vkQueuePresentKHR>(device, "vkQueuePresentKHR");
}

struct vulkan::device_funcs vulkan::deviceFuncs;
struct vulkan::instance_funcs vulkan::instanceFuncs;

template <typename T>
static T getInstanceProc(VkInstance instance, const char *entrypoint)
{
	void *ret = vkGetInstanceProcAddr(instance, entrypoint);
	assert(ret != nullptr);
	return reinterpret_cast<T>(ret);
}

void vulkan::instanceFuncsInit(VkInstance instance)
{
	instanceFuncs.vkCreateSwapchainKHR = getInstanceProc<PFN_vkCreateSwapchainKHR >(instance, "vkCreateSwapchainKHR");

	instanceFuncs.vkCreateDebugReportCallbackEXT = getInstanceProc<PFN_vkCreateDebugReportCallbackEXT>(instance, "vkCreateDebugReportCallbackEXT");
	instanceFuncs.vkDestroyDebugReportCallbackEXT = getInstanceProc<PFN_vkDestroyDebugReportCallbackEXT>(instance, "vkDestroyDebugReportCallbackEXT");
	instanceFuncs.vkDebugReportMessageEXT = getInstanceProc<PFN_vkDebugReportMessageEXT>(instance, "vkDebugReportMessageEXT");
}
