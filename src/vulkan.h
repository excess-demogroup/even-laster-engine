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

#include "core/core.h"

namespace vulkan
{
	extern VkInstance instance;
	extern VkDevice device;
	extern VkPhysicalDevice physicalDevice;
	extern VkPhysicalDeviceFeatures enabledFeatures;
	extern VkPhysicalDeviceProperties deviceProperties;
	extern VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	extern VkQueue graphicsQueue;
	extern uint32_t graphicsQueueIndex;

	extern VkCommandPool setupCommandPool;

	extern VkDebugReportCallbackEXT debugReportCallback;

	void instanceInit(const char *appName, const std::vector<const char *> &enabledExtensions);
	void deviceInit(VkPhysicalDevice physicalDevice, std::function<bool(VkInstance, VkPhysicalDevice, uint32_t)> usableQueue);

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

	static VkFormat findBestFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			switch (tiling) {
			case VK_IMAGE_TILING_LINEAR:
				if ((props.linearTilingFeatures & features) == features)
					return format;
				break;
			case VK_IMAGE_TILING_OPTIMAL:
				if ((props.optimalTilingFeatures & features) == features)
					return format;
				break;
			default:
				unreachable("unexpected tiling mode");
			}
		}

		throw std::runtime_error("no supported format!");
	}

	static VkFence createFence(VkFenceCreateFlags flags)
	{
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = flags;

		VkFence ret;
		VkResult err = vkCreateFence(device, &fenceCreateInfo, nullptr, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	static VkSemaphore createSemaphore()
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkSemaphore ret;
		VkResult err = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	static void setViewport(VkCommandBuffer commandBuffer, float x, float y, float width, float height)
	{
		VkViewport viewport = {};
		viewport.x = x;
		viewport.y = y;
		viewport.height = height;
		viewport.width = width;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	}

	static void setScissor(VkCommandBuffer commandBuffer, int x, int y, int width, int height)
	{
		VkRect2D scissor = {};
		scissor.offset.x = x;
		scissor.offset.y = y;
		scissor.extent.width = width;
		scissor.extent.height = height;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	static VkDescriptorPool createDescriptorPool(const std::vector<VkDescriptorPoolSize> &poolSizes, int maxSets)
	{
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
		descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
		descriptorPoolCreateInfo.maxSets = 1;

		VkDescriptorPool descriptorPool;
		auto err = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
		assert(err == VK_SUCCESS);

		return descriptorPool;
	}

	static VkCommandPool createCommandPool(uint32_t queueFamilyIndex)
	{
		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VkCommandPool commandPool;
		auto err = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
		assert(err == VK_SUCCESS);

		return commandPool;
	}

	static VkDescriptorSet allocateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

		VkDescriptorSet descriptorSet;
		auto err = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
		assert(err == VK_SUCCESS);

		return descriptorSet;
	}


	void instanceFuncsInit(VkInstance instance);
};

#endif // VULKAN_H
