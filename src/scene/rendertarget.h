#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include "../vulkan.h"

class RenderTargetBase {
protected:
	RenderTargetBase(VkFormat format, int width, int height, VkImageUsageFlags usage, VkImageAspectFlags aspect) :
		width(width),
		height(height)
	{
		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.extent = { (uint32_t)width, (uint32_t)height, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = usage;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult err = vkCreateImage(device, &imageCreateInfo, nullptr, &image);
		assert(err == VK_SUCCESS);

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, image, &memoryRequirements);

		auto memoryTypeIndex = getMemoryTypeIndex(memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto deviceMemory = allocateDeviceMemory(memoryRequirements.size, memoryTypeIndex);

		err = vkBindImageMemory(device, image, deviceMemory, 0);
		assert(err == VK_SUCCESS);

		VkImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask = aspect;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		imageView = createImageView(image, VK_IMAGE_VIEW_TYPE_2D, format, subresourceRange);
	}

public:

	int getWidth() { return width; }
	int getHeight() { return height; }

	VkImage getImage() { return image; }
	VkImageView getImageView() { return imageView; }

private:
	int width, height;
	VkImage image;
	VkImageView imageView;
};

class DepthRenderTarget : public RenderTargetBase
{
public:
	DepthRenderTarget(VkFormat format, int width, int height, VkImageUsageFlags usage = 0) :
		RenderTargetBase(format, width, height, usage | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT)
	{
	}
};

#endif // RENDERTARGET_H
