#include "buffer.h"

using namespace vulkan;

Buffer::Buffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags)
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

Buffer::~Buffer()
{
	vkDestroyBuffer(device, buffer, nullptr);
	vkFreeMemory(device, deviceMemory, nullptr);
}

void Buffer::uploadFromStagingBuffer(const StagingBuffer &stagingBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
{
	auto commandBuffer = allocateCommandBuffers(setupCommandPool, 1)[0];

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkResult err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	assert(err == VK_SUCCESS);

	VkBufferCopy bufferCopy = {};
	bufferCopy.srcOffset = srcOffset;
	bufferCopy.dstOffset = dstOffset;
	bufferCopy.size = size;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer.getBuffer(), buffer, 1, &bufferCopy);

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