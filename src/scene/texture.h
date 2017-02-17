#ifndef TEXTURE_H
#define TEXTURE_H

class Buffer {
public:
	Buffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags)
	{
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = usageFlags;

		VkResult err = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer);
		assert(err == VK_SUCCESS);

		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

		deviceMemory = allocateDeviceMemory(memoryRequirements, memoryPropertyFlags);

		err = vkBindBufferMemory(device, buffer, deviceMemory, 0);
		assert(err == VK_SUCCESS);
	}

	~Buffer()
	{
		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, deviceMemory, nullptr);
	}

	void *map(VkDeviceSize offset, VkDeviceSize size)
	{
		void *ret;
		VkResult err = vkMapMemory(device, deviceMemory, 0, size, 0, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	void unmap()
	{
		vkUnmapMemory(device, deviceMemory);
	}

	VkBuffer getBuffer() const
	{
		return buffer;
	}

	VkDeviceMemory getDeviceMemory() const
	{
		return deviceMemory;
	}

	static uint32_t getMemoryTypeIndex(const VkMemoryRequirements &memoryRequirements, VkMemoryPropertyFlags propertyFlags)
	{
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

		for (auto i = 0u; i < VK_MAX_MEMORY_TYPES; i++) {
			if (((memoryRequirements.memoryTypeBits >> i) & 1) == 1) {
				if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
					return i;
					break;
				}
			}
		}

		assert(false);
		// critical error!
		throw std::runtime_error("invalid memory type!");
	}

	static VkDeviceMemory allocateDeviceMemory(const VkMemoryRequirements &memoryRequirements, VkMemoryPropertyFlags propertyFlags)
	{
		VkMemoryAllocateInfo memoryAllocateInfo = {};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.allocationSize = memoryRequirements.size;
		memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirements, propertyFlags);

		VkDeviceMemory deviceMemory;
		VkResult err = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceMemory);
		assert(err == VK_SUCCESS);

		return deviceMemory;
	}

private:
	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
};

class StagingBuffer : public Buffer {
public:
	StagingBuffer(VkDeviceSize size) : Buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
	}
};

class TextureBase {
protected:
	TextureBase(VkFormat format, VkImageType imageType, VkImageViewType imageViewType, int width, int height, int depth, int mipLevels = 1, int arrayLayers = 1, bool useStaging = true) :
		format(format),
		imageType(imageType),
		imageViewType(imageViewType),
		baseWidth(width),
		baseHeight(height),
		baseDepth(depth)
	{
		assert(width > 0);
		assert(height > 0);
		assert(depth > 0);
		assert(mipLevels > 0);
		assert(arrayLayers > 0);

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = imageType;
		imageCreateInfo.format = format;
		imageCreateInfo.extent = { width, height, depth };
		imageCreateInfo.mipLevels = mipLevels;
		imageCreateInfo.arrayLayers = arrayLayers;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = useStaging ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		// imageCreateInfo.queueFamilyIndexCount - only needed if imageCreateInfo.sharingMode == VK_SHARING_MODE_CONCURRENT
		// imageCreateInfo.pQueueFamilyIndices - only needed if imageCreateInfo.sharingMode == VK_SHARING_MODE_CONCURRENT
		imageCreateInfo.initialLayout = useStaging ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PREINITIALIZED;

		if (useStaging)
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		VkResult err = vkCreateImage(device, &imageCreateInfo, nullptr, &image);
		assert(err == VK_SUCCESS);

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, image, &memoryRequirements);

		deviceMemory = Buffer::allocateDeviceMemory(memoryRequirements, useStaging ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		err = vkBindImageMemory(device, image, deviceMemory, 0);
		assert(err == VK_SUCCESS);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = image;
		imageViewCreateInfo.viewType = imageViewType;
		imageViewCreateInfo.format = format;
		imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = arrayLayers;
		imageViewCreateInfo.subresourceRange.levelCount = mipLevels;

		err = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView);
		assert(err == VK_SUCCESS);
	}

public:

	static int mipSize(int size, int mipLevel)
	{
		assert(mipLevel >= 0);
		return std::max(size >> mipLevel, 1);
	}

	void uploadFromStagingBuffer(StagingBuffer *stagingBuffer, int mipLevel = 0, int arrayLayer = 0)
	{
		assert(stagingBuffer != nullptr);

		auto commandBuffer = allocateCommandBuffers(setupCommandPool, 1)[0];

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VkResult err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		assert(err == VK_SUCCESS);

		VkImageMemoryBarrier imageBarrier = {};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

		imageBarrier.image = image;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

		imageBarrier.dstQueueFamilyIndex = graphicsQueueIndex;
		imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		imageBarrier.srcQueueFamilyIndex = graphicsQueueIndex;
		imageBarrier.srcAccessMask = 0;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.baseMipLevel = mipLevel;
		imageBarrier.subresourceRange.baseArrayLayer = arrayLayer;
		imageBarrier.subresourceRange.levelCount = 1;
		imageBarrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier);

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.baseArrayLayer = arrayLayer;
		copyRegion.imageSubresource.mipLevel = mipLevel;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset = { 0, 0, 0 };
		copyRegion.imageExtent.width = mipSize(baseWidth, mipLevel);
		copyRegion.imageExtent.height = mipSize(baseHeight, mipLevel);
		copyRegion.imageExtent.depth = mipSize(baseDepth, mipLevel);

		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer->getBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		err = vkEndCommandBuffer(commandBuffer);
		assert(err == VK_SUCCESS);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Submit draw command buffer
		err = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(err == VK_SUCCESS);
	}

	VkImageView getImageView()
	{
		return imageView;
	}

	VkSubresourceLayout getSubresourceLayout(int mipLevel = 0, int arrayLayer = 0)
	{
		VkImageSubresource subRes = {};
		subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subRes.mipLevel = mipLevel;
		subRes.arrayLayer = arrayLayer;

		VkSubresourceLayout ret;
		vkGetImageSubresourceLayout(device, image, &subRes, &ret);
		return ret;
	}

	void *map(VkDeviceSize offset, VkDeviceSize size)
	{
		void *ret;
		VkResult err = vkMapMemory(device, deviceMemory, offset, size, 0, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	void unmap()
	{
		vkUnmapMemory(device, deviceMemory);
	}

protected:
	VkFormat format;
	VkImageType imageType;
	VkImageViewType imageViewType;
	int baseWidth, baseHeight, baseDepth;

	VkImage image;
	VkImageView imageView;
	VkDeviceMemory deviceMemory;
};

class Texture2D : public TextureBase {
public:
	Texture2D(VkFormat format, int width, int height, int mipLevels = 1, int arrayLayers = 1, bool useStaging = true) :
		TextureBase(format, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, width, height, 1, mipLevels, arrayLayers, useStaging)
	{
	}
};

#endif // TEXTURE_H