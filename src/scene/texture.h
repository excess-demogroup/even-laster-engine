#ifndef TEXTURE_H
#define TEXTURE_H

#include <algorithm>
#include "buffer.h"
#include "../core/core.h"

class TextureBase {
protected:
	TextureBase(VkFormat format, VkImageType imageType, VkImageViewType imageViewType, int width, int height, int depth, int mipLevels = 1, int arrayLayers = 1, bool useStaging = true);

public:

	static int mipSize(int size, int mipLevel)
	{
		assert(mipLevel >= 0);
		return std::max(size >> mipLevel, 1);
	}

	static int maxMipLevels(int baseSize)
	{
		return 32 - clz(baseSize);
	}

	int getWidth(int level = 0) { return mipSize(baseWidth, level); }
	int getHeight(int level = 0) { return mipSize(baseHeight, level); }
	int getDepth(int level = 0) { return mipSize(baseDepth, level); }

	int getMipLevels() const { return mipLevels; }
	int getArrayLayers() const { return arrayLayers; }

	void uploadFromStagingBuffer(StagingBuffer *stagingBuffer, int mipLevel = 0, int arrayLayer = 0);

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
		vkGetImageSubresourceLayout(vulkan::device, image, &subRes, &ret);
		return ret;
	}

	VkDescriptorImageInfo getDescriptorImageInfo(VkSampler textureSampler)
	{
		VkDescriptorImageInfo descriptorImageInfo;
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // TODO: make *sure* of this!
		descriptorImageInfo.imageView = imageView;
		descriptorImageInfo.sampler = textureSampler;
		return descriptorImageInfo;
	}

	void *map(VkDeviceSize offset, VkDeviceSize size)
	{
		void *ret;
		VkResult err = vkMapMemory(vulkan::device, deviceMemory, offset, size, 0, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	void unmap()
	{
		vkUnmapMemory(vulkan::device, deviceMemory);
	}

protected:
	int baseWidth, baseHeight, baseDepth;
	int mipLevels, arrayLayers;

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

class Texture2DArray : public TextureBase {
public:
	Texture2DArray(VkFormat format, int width, int height, int arrayLayers, int mipLevels = 1, bool useStaging = true) :
		TextureBase(format, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY, width, height, 1, mipLevels, arrayLayers, useStaging)
	{
	}
};

class TextureCube : public TextureBase {
public:
	TextureCube(VkFormat format, int size, int mipLevels = 1) :
		TextureBase(format, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_CUBE, size, size, 1, mipLevels, 6, true)
	{
	}
};

class Texture3D : public TextureBase {
public:
	Texture3D(VkFormat format, int width, int height, int depth, int mipLevels = 1) :
		TextureBase(format, VK_IMAGE_TYPE_3D, VK_IMAGE_VIEW_TYPE_3D, width, height, depth, mipLevels, 1, true)
	{
	}
};

#endif // TEXTURE_H
