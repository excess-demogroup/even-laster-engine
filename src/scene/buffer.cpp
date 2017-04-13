#include "buffer.h"

Buffer::Buffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags)
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

Buffer::~Buffer()
{
	vkDestroyBuffer(vulkan::device, buffer, nullptr);
	vkFreeMemory(vulkan::device, deviceMemory, nullptr);
}

void Buffer::uploadFromStagingBuffer(StagingBuffer *stagingBuffer, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkDeviceSize size)
{
	assert(stagingBuffer != nullptr);

	auto commandBuffer = vulkan::allocateCommandBuffers(vulkan::setupCommandPool, 1)[0];

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkResult err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	assert(err == VK_SUCCESS);

	VkBufferCopy bufferCopy = {};
	bufferCopy.srcOffset = srcOffset;
	bufferCopy.dstOffset = dstOffset;
	bufferCopy.size = size;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer->getBuffer(), buffer, 1, &bufferCopy);

	err = vkEndCommandBuffer(commandBuffer);
	assert(err == VK_SUCCESS);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Submit draw command buffer
	err = vkQueueSubmit(vulkan::graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	assert(err == VK_SUCCESS);
}