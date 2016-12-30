#include "vulkan.h"

class SwapChain
{
public:
	SwapChain(VkSurfaceKHR surface, int width, int height);

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
	uint32_t imageCount;
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
};
