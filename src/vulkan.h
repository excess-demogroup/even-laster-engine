#ifndef VULKAN_H
#define VULKAN_H

#define VK_PROTOTYPES

#include <vulkan/vulkan.h>

#include <algorithm>
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
	extern uint32_t graphicsQueueFamily;

	extern VkCommandPool setupCommandPool;

	extern VkDebugReportCallbackEXT debugReportCallback;

	void instanceInit(const std::string &appName, const std::vector<const char *> &enabledExtensions);
	void deviceInit(VkPhysicalDevice physicalDevice, std::function<bool(VkInstance, VkPhysicalDevice, uint32_t)> usableQueue);

	extern struct instance_funcs {
		PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
		PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
	} instanceFuncs;

	inline VkDeviceSize alignSize(VkDeviceSize value, VkDeviceSize alignment)
	{
		return ((value + alignment - 1) / alignment) * alignment;
	}

	inline uint32_t getMemoryTypeIndex(const VkMemoryRequirements &memoryRequirements, VkMemoryPropertyFlags propertyFlags)
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

	inline VkDeviceMemory allocateDeviceMemory(VkDeviceSize size, uint32_t memoryTypeIndex)
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

	inline std::vector<VkCommandBuffer> allocateCommandBuffers(VkCommandPool commandPool, size_t commandBufferCount)
	{
		assert(commandBufferCount < UINT32_MAX);
		VkCommandBufferAllocateInfo commandAllocInfo = {};
		commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandAllocInfo.commandPool = commandPool;
		commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandAllocInfo.commandBufferCount = uint32_t(commandBufferCount);

		std::vector<VkCommandBuffer> commandBuffers;
		commandBuffers.resize(commandBufferCount);
		VkResult err = vkAllocateCommandBuffers(device, &commandAllocInfo, commandBuffers.data());
		assert(err == VK_SUCCESS);

		return commandBuffers;
	}

	inline VkFormat findBestFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (auto format : candidates) {
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

	inline VkFence createFence(VkFenceCreateFlags flags)
	{
		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = flags;

		VkFence ret;
		VkResult err = vkCreateFence(device, &fenceCreateInfo, nullptr, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	inline VkSemaphore createSemaphore()
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkSemaphore ret;
		VkResult err = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	inline void setViewport(VkCommandBuffer commandBuffer, float x, float y, float width, float height)
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

	inline void setScissor(VkCommandBuffer commandBuffer, int x, int y, int width, int height)
	{
		VkRect2D scissor = {};
		scissor.offset.x = x;
		scissor.offset.y = y;
		scissor.extent.width = width;
		scissor.extent.height = height;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	inline void imageBarrier(VkCommandBuffer commandBuffer, VkImage image,
		const VkImageSubresourceRange &subresourceRange,
		VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
		VkAccessFlags srcAccess, VkAccessFlags dstAccess,
		VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t oldQueueFamily = VK_QUEUE_FAMILY_IGNORED,
		uint32_t newQueueFamily = VK_QUEUE_FAMILY_IGNORED)
	{
		VkImageMemoryBarrier imageBarrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			nullptr,
			srcAccess,
			dstAccess,
			oldLayout,
			newLayout,
			oldQueueFamily,
			newQueueFamily,
			image,
			subresourceRange
		};

		vkCmdPipelineBarrier(
			commandBuffer, srcStage, dstStage, 0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier
		);
	}

	inline void imageBarrier(VkCommandBuffer commandBuffer, VkImage image,
		VkImageAspectFlags imageAspectFlags,
		VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
		VkAccessFlags srcAccess, VkAccessFlags dstAccess,
		VkImageLayout oldLayout, VkImageLayout newLayout,
		uint32_t oldQueueFamily = VK_QUEUE_FAMILY_IGNORED,
		uint32_t newQueueFamily = VK_QUEUE_FAMILY_IGNORED)
	{
		VkImageSubresourceRange subresourceRange = {
			imageAspectFlags,
			0, VK_REMAINING_MIP_LEVELS,
			0, VK_REMAINING_ARRAY_LAYERS
		};

		imageBarrier(commandBuffer,
			image, subresourceRange,
			srcStage, dstStage,
			srcAccess, dstAccess,
			oldLayout, newLayout,
			oldQueueFamily, newQueueFamily);
	}

	inline void blitImage(
		VkCommandBuffer commandBuffer,
		VkImage srcImage, VkImage dstImage,
		const std::vector<VkImageBlit> &imageBlits,
		VkFilter filter = VK_FILTER_NEAREST,
		VkImageLayout srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		assert(imageBlits.size() < UINT32_MAX);
		vkCmdBlitImage(commandBuffer,
			srcImage, srcLayout,
			dstImage, dstLayout,
			uint32_t(imageBlits.size()), imageBlits.data(),
			filter);
	}

	inline void blitImage(
		VkCommandBuffer commandBuffer,
		VkImage srcImage, VkImage dstImage,
		int width, int height,
		VkImageSubresourceLayers srcSubresourceLayers,
		VkImageSubresourceLayers dstSubresourceLayers,
		VkFilter filter = VK_FILTER_NEAREST,
		VkImageLayout srcLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		VkImageBlit imageBlit = {};
		imageBlit.srcSubresource = srcSubresourceLayers;
		imageBlit.srcOffsets[1].x = int32_t(width);
		imageBlit.srcOffsets[1].y = int32_t(height);
		imageBlit.srcOffsets[1].z = 1;

		imageBlit.dstSubresource = dstSubresourceLayers;
		imageBlit.dstOffsets[1].x = width;
		imageBlit.dstOffsets[1].y = height;
		imageBlit.dstOffsets[1].z = 1;

		blitImage(commandBuffer,
			srcImage, dstImage,
			{ imageBlit }, filter,
			srcLayout, dstLayout);
	}

	inline VkDescriptorPool createDescriptorPool(const std::vector<VkDescriptorPoolSize> &poolSizes, size_t maxSets)
	{
		assert(poolSizes.size() < UINT32_MAX);
		assert(maxSets < UINT32_MAX);

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.poolSizeCount = uint32_t(poolSizes.size());
		descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
		descriptorPoolCreateInfo.maxSets = uint32_t(maxSets);

		VkDescriptorPool descriptorPool;
		auto err = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
		assert(err == VK_SUCCESS);

		return descriptorPool;
	}

	inline VkCommandPool createCommandPool(uint32_t queueFamilyIndex)
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

	inline VkDescriptorSet allocateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout)
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

#define IDENTITY_SWIZZLE    \
{                           \
	VK_COMPONENT_SWIZZLE_R, \
	VK_COMPONENT_SWIZZLE_G, \
	VK_COMPONENT_SWIZZLE_B, \
	VK_COMPONENT_SWIZZLE_A  \
}

	inline VkImageView createImageView(VkImage image, VkImageViewType viewType, VkFormat format, const VkImageSubresourceRange &subresourceRange, VkComponentMapping components = IDENTITY_SWIZZLE)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = image;
		imageViewCreateInfo.viewType = viewType;
		imageViewCreateInfo.format = format;
		imageViewCreateInfo.components = components;
		imageViewCreateInfo.subresourceRange = subresourceRange;

		VkImageView imageView;
		auto err = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView);
		assert(err == VK_SUCCESS);

		return imageView;
	}

	inline VkSampler createSampler(float maxLod, bool repeat, bool wantAnisotropy)
	{
		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = maxLod;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		if (wantAnisotropy && vulkan::enabledFeatures.samplerAnisotropy) {
			samplerCreateInfo.maxAnisotropy = std::min(8.0f, vulkan::deviceProperties.limits.maxSamplerAnisotropy);
			samplerCreateInfo.anisotropyEnable = VK_TRUE;
		}

		VkSampler textureSampler;
		VkResult err = vkCreateSampler(device, &samplerCreateInfo, nullptr, &textureSampler);
		assert(err == VK_SUCCESS);

		return textureSampler;
	}

	inline VkFramebuffer createFramebuffer(int width, int height, int layers, std::vector<VkImageView> attachments, VkRenderPass renderPass)
	{
		assert(width > 0);
		assert(height > 0);
		assert(layers > 0);
		assert(attachments.size() > 0 && attachments.size() < UINT32_MAX);

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = uint32_t(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = uint32_t(width);
		framebufferCreateInfo.height = uint32_t(height);
		framebufferCreateInfo.layers = uint32_t(layers);

		VkFramebuffer framebuffer;
		auto err = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer);
		assert(err == VK_SUCCESS);
		return framebuffer;
	}

	inline VkPipelineLayout createPipelineLayout(const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts, const std::vector<VkPushConstantRange> &pushConstantRanges)
	{
		assert(descriptorSetLayouts.size() < UINT32_MAX);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		pipelineLayoutCreateInfo.setLayoutCount = uint32_t(descriptorSetLayouts.size());
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

		pipelineLayoutCreateInfo.pushConstantRangeCount = uint32_t(pushConstantRanges.size());
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

		VkPipelineLayout pipelineLayout;
		auto err = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		assert(err == VK_SUCCESS);
		return pipelineLayout;
	}

	inline VkDescriptorSetLayout createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding> &layoutBindings, VkDescriptorSetLayoutCreateFlags flags = 0)
	{
		assert(layoutBindings.size() < UINT32_MAX);

		VkDescriptorSetLayoutCreateInfo desciptorSetLayoutCreateInfo = {};
		desciptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

		desciptorSetLayoutCreateInfo.flags = flags;

		desciptorSetLayoutCreateInfo.bindingCount = uint32_t(layoutBindings.size());
		desciptorSetLayoutCreateInfo.pBindings = layoutBindings.data();

		VkDescriptorSetLayout descriptorSetLayout;
		auto err = vkCreateDescriptorSetLayout(device, &desciptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
		assert(err == VK_SUCCESS);

		return descriptorSetLayout;
	}

	void instanceFuncsInit(VkInstance instance);
};

#endif // VULKAN_H
