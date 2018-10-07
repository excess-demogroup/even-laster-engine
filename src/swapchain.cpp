#include "swapchain.h"

#include "vulkan.h"
#include <assert.h>
#include <algorithm>
#include <stdexcept>

using namespace vulkan;

using std::vector;
using std::runtime_error;

static vector<VkSurfaceFormatKHR> getSurfaceFormats(VkSurfaceKHR surface)
{
	uint32_t surfaceFormatCount = 0;
	VkResult err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
	assert(err == VK_SUCCESS);
	assert(surfaceFormatCount > 0);

	vector<VkSurfaceFormatKHR> surfaceFormats;
	surfaceFormats.resize(surfaceFormatCount);
	err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
	assert(err == VK_SUCCESS);

	return surfaceFormats;
}

static vector<VkPresentModeKHR> getPresentModes(VkSurfaceKHR surface)
{
	uint32_t presentModeCount = 0;
	VkResult err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	assert(err == VK_SUCCESS);
	assert(presentModeCount > 0);

	vector<VkPresentModeKHR> presentModes;
	presentModes.resize(presentModeCount);
	err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
	assert(err == VK_SUCCESS);

	return presentModes;
}

SwapChain::SwapChain(VkSurfaceKHR surface, int width, int height, VkImageUsageFlags imageUsage) :
	swapChain(VK_NULL_HANDLE)
{
	VkBool32 surfaceSupported = VK_FALSE;
	VkResult err = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueFamily, surface, &surfaceSupported);
	assert(err == VK_SUCCESS);
	assert(surfaceSupported == VK_TRUE);

	vector<VkSurfaceFormatKHR> surfaceFormats = getSurfaceFormats(surface);

	if (surfaceFormats.size() == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
		surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
		surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	} else {
		// find sRGB color format
		auto result = std::find_if(surfaceFormats.begin(), surfaceFormats.end(), [&](const VkSurfaceFormatKHR &format) {
			VkFormatProperties formatProperties;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format.format, &formatProperties);

			if ((imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0 &&
				(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
				return false;

			if ((imageUsage & VK_IMAGE_USAGE_STORAGE_BIT) != 0 &&
				(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0)
				return false;

			if (format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return false;

			switch (format.format) {
			case VK_FORMAT_R8G8B8_SRGB:
			case VK_FORMAT_B8G8R8_SRGB:
			case VK_FORMAT_R8G8B8A8_SRGB:
			case VK_FORMAT_B8G8R8A8_SRGB:
			case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
				return true;

			default:
				return false;
			}
		});

		if (result == surfaceFormats.end())
			throw runtime_error("Unable to find an sRGB surface format");

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
	swapchainCreateInfo.imageUsage = imageUsage;

	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;

	auto presentModes = getPresentModes(surface);
	swapchainCreateInfo.presentMode = presentModes[0];
	for (auto presentMode : presentModes)
		if (presentMode == VK_PRESENT_MODE_FIFO_KHR)
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
		VkImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		imageViews[i] = createImageView(images[i], VK_IMAGE_VIEW_TYPE_2D, surfaceFormat.format, subresourceRange);
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
