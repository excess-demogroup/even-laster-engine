#include "buffer.h"

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