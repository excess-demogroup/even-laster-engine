#ifndef VULKAN_H
#define VULKAN_H

#define VK_PROTOTYPES

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>

#include <functional>
#include <vector>
#include <cassert>

namespace vulkan
{
	extern VkInstance instance;
	extern VkDevice device;
	extern VkPhysicalDevice physicalDevice;
	extern VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	extern VkQueue graphicsQueue;
	extern uint32_t graphicsQueueIndex;

	extern VkCommandPool setupCommandPool;

	extern VkDebugReportCallbackEXT debugReportCallback;

	void instanceInit(const char *appName, const std::vector<const char *> &enabledExtensions);
	VkResult deviceInit(VkPhysicalDevice physicalDevice, std::function<bool(VkInstance, VkPhysicalDevice, uint32_t)> usableQueue);

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

	static VkDeviceMemory allocateDeviceMemory(VkDeviceSize size, uint32_t memoryTypeIndex)
	{
		VkMemoryAllocateInfo memoryAllocateInfo = {};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = size;
		memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

		VkDeviceMemory deviceMemory;
		VkResult err = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceMemory);
		assert(err == VK_SUCCESS);

		return deviceMemory;
	}

	static VkCommandBuffer *allocateCommandBuffers(VkCommandPool commandPool, int commandBufferCount)
	{
		VkCommandBufferAllocateInfo commandAllocInfo = {};
		commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandAllocInfo.commandPool = commandPool;
		commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandAllocInfo.commandBufferCount = commandBufferCount;

		auto commandBuffers = new VkCommandBuffer[commandBufferCount];
		VkResult err = vkAllocateCommandBuffers(device, &commandAllocInfo, commandBuffers);
		assert(err == VK_SUCCESS);

		return commandBuffers;
	}

	void instanceFuncsInit(VkInstance instance);
};

#endif // VULKAN_H
