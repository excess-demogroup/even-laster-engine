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

#endif // 1BUFFER_H
