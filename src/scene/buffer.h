#ifndef BUFFER_H
#define BUFFER_H

#include "../vulkan.h"

#include <cstring>

class StagingBuffer;

class Buffer {
public:
	Buffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags)
	{
		VkBufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = size;
		bufferCreateInfo.usage = usageFlags;

		VkResult err = vkCreateBuffer(vulkan::device, &bufferCreateInfo, nullptr, &buffer);
		assert(err == VK_SUCCESS);

		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(vulkan::device, buffer, &memoryRequirements);

		auto memoryTypeIndex = vulkan::getMemoryTypeIndex(memoryRequirements, memoryPropertyFlags);
		deviceMemory = vulkan::allocateDeviceMemory(memoryRequirements.size, memoryTypeIndex);

		err = vkBindBufferMemory(vulkan::device, buffer, deviceMemory, 0);
		assert(err == VK_SUCCESS);
	}

	~Buffer()
	{
		vkDestroyBuffer(vulkan::device, buffer, nullptr);
		vkFreeMemory(vulkan::device, deviceMemory, nullptr);
	}

	void *map(VkDeviceSize offset, VkDeviceSize size)
	{
		void *ret;
		VkResult err = vkMapMemory(vulkan::device, deviceMemory, 0, size, 0, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	void unmap()
	{
		vkUnmapMemory(vulkan::device, deviceMemory);
	}

	void uploadMemory(VkDeviceSize offset, void *data, VkDeviceSize size)
	{
		auto mappedUniformMemory = map(offset, size);
		memcpy(mappedUniformMemory, data, (size_t)size);
		unmap();
	}

	VkBuffer getBuffer() const
	{
		return buffer;
	}

	VkDeviceMemory getDeviceMemory() const
	{
		return deviceMemory;
	}

	void uploadFromStagingBuffer(StagingBuffer *stagingBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);

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

#endif // 1BUFFER_H
