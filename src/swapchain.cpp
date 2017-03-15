#include "swapchain.h"

#include "vulkan.h"
#include <assert.h>
#include <algorithm>

using namespace vulkan;

static std::vector<VkSurfaceFormatKHR> getSurfaceFormats(VkSurfaceKHR surface)
{
	uint32_t surfaceFormatCount = 0;
	VkResult err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
	assert(err == VK_SUCCESS);
	assert(surfaceFormatCount > 0);

	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	surfaceFormats.resize(surfaceFormatCount);
	err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
	assert(err == VK_SUCCESS);

	return surfaceFormats;
}

static std::vector<VkPresentModeKHR> getPresentModes(VkSurfaceKHR surface)
{
	uint32_t presentModeCount = 0;
	VkResult err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	assert(err == VK_SUCCESS);
	assert(presentModeCount > 0);

	std::vector<VkPresentModeKHR> presentModes;
	presentModes.resize(presentModeCount);
	err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
	assert(err == VK_SUCCESS);

	return presentModes;
}

SwapChain::SwapChain(VkSurfaceKHR surface, int width, int height) :
	swapChain(VK_NULL_HANDLE)
{
	VkBool32 surfaceSupported = VK_FALSE;
	VkResult err = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueIndex, surface, &surfaceSupported);
	assert(err == VK_SUCCESS);
	assert(surfaceSupported == VK_TRUE);

	std::vector<VkSurfaceFormatKHR> surfaceFormats = getSurfaceFormats(surface);

	if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
		surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
		surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	} else {
		// find sRGB color format
		auto result = std::find_if(surfaceFormats.begin(), surfaceFormats.end(), [](const VkSurfaceFormatKHR &format) {
			if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return false;

			switch (format.format) {
			case VK_FORMAT_R8G8B8_SRGB:
			case VK_FORMAT_B8G8R8_SRGB:
			case VK_FORMAT_R8G8B8A8_SRGB:
			case VK_FORMAT_B8G8R8A8_SRGB:
			case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
				return true;
			}

			return false;
		});

		if (result == surfaceFormats.end())
			throw std::runtime_error("Unable to find an sRGB surface format");

		surfaceFormat = *result;
		assert(surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
	}

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
	assert(err == VK_SUCCESS);

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount != 0)
		swapchainCreateInfo.minImageCount = std::min(swapchainCreateInfo.minImageCount, surfaceCapabilities.maxImageCount);
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = { (uint32_t)width, (uint32_t)height };
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;

	auto presentModes = getPresentModes(surface);
	swapchainCreateInfo.presentMode = presentModes[0];
	for (auto presentMode : presentModes)
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			swapchainCreateInfo.presentMode = presentMode;

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
