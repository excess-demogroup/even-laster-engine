#include "swapchain.h"

#include "vulkan.h"
#include <assert.h>

using namespace vulkan;

SwapChain::SwapChain(VkSurfaceKHR surface, int width, int height) :
	swapChain(VK_NULL_HANDLE)
{
	VkBool32 surfaceSupported = VK_FALSE;
	VkResult err = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueIndex, surface, &surfaceSupported);
	assert(err == VK_SUCCESS);
	assert(surfaceSupported == VK_TRUE);

	VkSurfaceCapabilitiesKHR surfCaps;
	err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);
	assert(err == VK_SUCCESS);

	uint32_t surfaceFormatCount = 0;
	err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
	assert(err == VK_SUCCESS);
	assert(surfaceFormatCount > 0);
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	surfaceFormats.resize(surfaceFormatCount);
	err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
	assert(err == VK_SUCCESS);

	// find sRGB color format
	surfaceFormat = surfaceFormats[0];
	for (size_t i = 1; i < surfaceFormatCount; ++i) {
		if (surfaceFormats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			surfaceFormat = surfaceFormats[i];
			break;
		}
	}
	assert(surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR);

	uint32_t presentModeCount = 0;
	err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	assert(err == VK_SUCCESS);
	assert(presentModeCount > 0);
	auto presentModes = new VkPresentModeKHR[presentModeCount];
	err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);
	assert(err == VK_SUCCESS);

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = min(surfCaps.minImageCount + 1, surfCaps.maxImageCount);
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = { width, height };
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;

	swapchainCreateInfo.presentMode = presentModes[0];
	for (uint32_t i = 0; i < presentModeCount; ++i)
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			swapchainCreateInfo.presentMode = presentModes[i];

	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	swapchainCreateInfo.clipped = true;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	err = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapChain);
	assert(err == VK_SUCCESS);
	assert(swapChain != VK_NULL_HANDLE);

	uint32_t imageCount;
	err = vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
	assert(err == VK_SUCCESS);
	assert(imageCount > 0);

	// Get the swap chain images
	images.resize(imageCount);
	err = vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data());
	assert(err == VK_SUCCESS);

	imageViews.resize(imageCount);
	for (uint32_t i = 0; i < imageCount; i++) {
		VkImageViewCreateInfo imageView = {};
		imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageView.format = surfaceFormat.format;
		imageView.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		imageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageView.image = images[i];

		err = vkCreateImageView(device, &imageView, nullptr, &imageViews[i]);
		assert(err == VK_SUCCESS);
	}
}

uint32_t SwapChain::aquireNextImage(VkSemaphore presentCompleteSemaphore)
{
	uint32_t currentSwapImage;
	VkResult err = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, VK_NULL_HANDLE, &currentSwapImage);
	assert(err == VK_SUCCESS);

	return currentSwapImage;
}

void SwapChain::queuePresent(uint32_t currentSwapImage, const VkSemaphore *waitSemaphores, uint32_t numWaitSemaphores)
{
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &currentSwapImage;
	presentInfo.pWaitSemaphores = waitSemaphores;
	presentInfo.waitSemaphoreCount = numWaitSemaphores;
	VkResult err = vkQueuePresentKHR(graphicsQueue, &presentInfo);
	assert(err == VK_SUCCESS);
}
