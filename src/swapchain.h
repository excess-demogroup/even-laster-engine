#include "vkinstance.h"

#include <vector>

class SwapChain {
public:
	SwapChain(VkSurfaceKHR surface, int width, int height, VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

	const std::vector<VkImage> &getImages() const
	{
		return images;
	}

	const std::vector<VkImageView> &getImageViews() const
	{
		return imageViews;
	}

	const VkSurfaceFormatKHR &getSurfaceFormat() const
	{
		return surfaceFormat;
	}

	uint32_t aquireNextImage(VkSemaphore presentCompleteSemaphore);

	void queuePresent(uint32_t currentSwapImage, const VkSemaphore *waitSemaphores, uint32_t numWaitSemaphores);

private:
	VkSurfaceFormatKHR surfaceFormat;
	VkSwapchainKHR swapChain;
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
};
