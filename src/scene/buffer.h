#ifndef BUFFER_H
#define BUFFER_H

#include "../vkinstance.h"

#include <cstring>

class StagingBuffer;

class Buffer {
public:
	Buffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags);
	~Buffer();

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

	void uploadMemory(VkDeviceSize offset, void *data, VkDeviceSize size)
	{
		auto mappedUniformMemory = map(offset, size);
		memcpy(mappedUniformMemory, data, (size_t)size);
		unmap();
	}

	VkBuffer getBuffer() const { return buffer; }
	VkDeviceSize getSize() const { return size; }

	VkDescriptorBufferInfo getDescriptorBufferInfo(VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE)
	{
		VkDescriptorBufferInfo descriptorBufferInfo;
		descriptorBufferInfo.buffer = buffer;
		descriptorBufferInfo.offset = offset;
		descriptorBufferInfo.range = range;
		return descriptorBufferInfo;
	}

	void uploadFromStagingBuffer(const StagingBuffer &stagingBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size);

private:
	VkBuffer buffer;
	VkDeviceSize size;
	VkDeviceMemory deviceMemory;
};

class UniformBuffer : public Buffer {
public:
	UniformBuffer(VkDeviceSize size) : Buffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
	}
};

class StagingBuffer : public Buffer {
public:
	StagingBuffer(VkDeviceSize size) : Buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
	}
};

class VertexBuffer : public Buffer {
public:
	VertexBuffer(VkDeviceSize size) : Buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	{
	}
};

class IndexBuffer : public Buffer {
public:
	IndexBuffer(VkDeviceSize size) : Buffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	{
	}
};

#endif // BUFFER_H
