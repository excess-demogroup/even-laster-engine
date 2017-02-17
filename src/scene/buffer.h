#ifndef BUFFER_H
#define BUFFER_H

#include "../vulkan.h"

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

		auto memoryTypeIndex = getMemoryTypeIndex(memoryRequirements, memoryPropertyFlags);
		deviceMemory = allocateDeviceMemory(memoryRequirements.size, memoryTypeIndex);

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
