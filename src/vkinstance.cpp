#include "core/core.h"

#include <assert.h>
#include <stdio.h>
#include <cstring>

#include "vkinstance.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace vulkan;

using std::vector;
using std::function;
using std::runtime_error;

VkInstance vulkan::instance;
VkDevice vulkan::device;
VkPhysicalDevice vulkan::physicalDevice;
VkPhysicalDeviceFeatures vulkan::enabledFeatures = { 0 };
VkPhysicalDeviceProperties vulkan::deviceProperties;
VkPhysicalDeviceMemoryProperties vulkan::deviceMemoryProperties;
uint32_t vulkan::graphicsQueueFamily = UINT32_MAX;
VkQueue vulkan::graphicsQueue;
VkCommandPool setupCommandPool;
VkDebugReportCallbackEXT vulkan::debugReportCallback;

#ifndef NDEBUG
static const char *validationLayerNames[] = {
	"VK_LAYER_LUNARG_standard_validation"
};
#endif

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

void vulkan::instanceInit(const std::string &appName, const vector<const char *> &enabledExtensions)
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName.c_str();
	appInfo.pEngineName = "very lastest engine ever";
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	assert(enabledExtensions.size() < UINT32_MAX);
	instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
	instanceCreateInfo.enabledExtensionCount = uint32_t(enabledExtensions.size());

#ifndef NDEBUG
	instanceCreateInfo.ppEnabledLayerNames = validationLayerNames;
	instanceCreateInfo.enabledLayerCount = ARRAY_SIZE(validationLayerNames);
#endif

	VkResult err = vkCreateInstance(&instanceCreateInfo, nullptr, &vulkan::instance);

	if (err == VK_ERROR_INCOMPATIBLE_DRIVER)
		throw runtime_error("Your GPU is from Hønefoss!");

	assert(err == VK_SUCCESS);

	instanceFuncsInit(vulkan::instance);

#ifndef NDEBUG
	VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo = {};
	debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	debugReportCallbackCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)messageCallback;
	debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	err = instanceFuncs.vkCreateDebugReportCallbackEXT(instance, &debugReportCallbackCreateInfo,
	                                                   nullptr, &debugReportCallback);
	assert(err == VK_SUCCESS);

	// SELF-TEST:
	// instanceFuncs.vkDebugReportMessageEXT(instance, VK_DEBUG_REPORT_WARNING_BIT_EXT, VK_DEBUG_REPORT_OBJECT_TYPE_UNKNOWN_EXT, nullptr, 0, 0, "self-test", "This is a dummy warning");
#endif
}

static uint32_t findQueueFamily(VkPhysicalDevice physicalDevice, VkQueueFlags requiredFlags, function<bool(VkInstance, VkPhysicalDevice, uint32_t)> usableQueue)
{
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	assert(queueFamilyCount > 0);

	VkQueueFamilyProperties *props = new VkQueueFamilyProperties[queueFamilyCount];
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, props);

	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if ((props[i].queueFlags & requiredFlags) == requiredFlags && usableQueue(instance, physicalDevice, i)) {
			delete[] props;
			return i;
		}
	}

	delete[] props;
	throw runtime_error("failed to find queue!");
}

void vulkan::deviceInit(VkPhysicalDevice physicalDevice, function<bool(VkInstance, VkPhysicalDevice, uint32_t)> usableQueue)
{
	vulkan::physicalDevice = physicalDevice;

	VkPhysicalDeviceFeatures physicalDeviceFeatures;
	vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

	enabledFeatures.samplerAnisotropy = physicalDeviceFeatures.samplerAnisotropy;

	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

	graphicsQueueFamily = findQueueFamily(physicalDevice, VK_QUEUE_GRAPHICS_BIT, usableQueue);

	VkDeviceQueueCreateInfo queueCreateInfo = {};
	float queuePriorities = 0.0f;
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriorities;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

	const char *enabledExtensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	deviceCreateInfo.enabledExtensionCount = ARRAY_SIZE(enabledExtensions);
	deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions;

#ifndef NDEBUG
	deviceCreateInfo.ppEnabledLayerNames = validationLayerNames;
	deviceCreateInfo.enabledLayerCount = ARRAY_SIZE(validationLayerNames);
#endif

	VkResult err = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
	assert(err == VK_SUCCESS);

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
	vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);

	setupCommandPool = createCommandPool(graphicsQueueFamily);
}

VkCommandBuffer vulkan::getSetupCommandBuffer()
{
	return allocateCommandBuffers(setupCommandPool, 1)[0];
}

void vulkan::setupCleanup()
{
	VkResult err = vkQueueWaitIdle(graphicsQueue);
	assert(err == VK_SUCCESS);
	vkDestroyCommandPool(device, setupCommandPool, nullptr);
}

template <typename T>
static T getDeviceProc(VkDevice device, const std::string &entrypoint)
{
	auto ret = reinterpret_cast<T>(vkGetDeviceProcAddr(device, entrypoint.c_str()));
	assert(ret != nullptr);
	return ret;
}

struct vulkan::instance_funcs vulkan::instanceFuncs;

template <typename T>
static T getInstanceProc(VkInstance instance, const std::string &entrypoint)
{
	auto ret = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, entrypoint.c_str()));
	assert(ret != nullptr);
	return ret;
}

void vulkan::instanceFuncsInit(VkInstance instance)
{
	instanceFuncs.vkCreateDebugReportCallbackEXT = getInstanceProc<PFN_vkCreateDebugReportCallbackEXT>(instance, "vkCreateDebugReportCallbackEXT");
	instanceFuncs.vkDestroyDebugReportCallbackEXT = getInstanceProc<PFN_vkDestroyDebugReportCallbackEXT>(instance, "vkDestroyDebugReportCallbackEXT");
	instanceFuncs.vkDebugReportMessageEXT = getInstanceProc<PFN_vkDebugReportMessageEXT>(instance, "vkDebugReportMessageEXT");
}
