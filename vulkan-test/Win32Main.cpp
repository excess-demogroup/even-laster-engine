// vulkan-test.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "vulkan.h"

using namespace vulkan;

#define MAX_LOADSTRING 100

// Global Variables:
static HWND hWnd;

// Forward declarations of functions included in this code module:
static ATOM registerWindowClass(HINSTANCE hInstance);
static BOOL initInstance(HINSTANCE, int);
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

static VkSurfaceKHR surface;

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	const char *appName = "some excess demo";

	// Initialize global strings
	ATOM windowClass = registerWindowClass(hInstance);

	int width = 1280, height = 720;

	// Perform application initialization:
	DWORD ws = WS_OVERLAPPEDWINDOW;
	RECT rect = { 0, 0, width, height };
	AdjustWindowRect(&rect, ws, FALSE);
	hWnd = CreateWindow((LPCSTR)windowClass, appName, ws, 0, 0, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		return FALSE;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	VkResult err = init(appName);
	if (err != VK_SUCCESS)
		return FALSE;

//	vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported);

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = hInstance;
	surfaceCreateInfo.hwnd = hWnd;
	err = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
	assert(err == VK_SUCCESS);

#if 0
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
	assert(queueCount >= 1);

	bool *supportsPresent = new bool[queueCount], anySupportsPresent = false;
	for (uint32_t i = 0; i < queueCount; i++) {
		VkBool32 tmp;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &tmp);
		supportsPresent[i] = !!tmp;
		anySupportsPresent = anySupportsPresent || supportsPresent[i];
	}
	assert(anySupportsPresent);
#endif

	VkSurfaceCapabilitiesKHR surfCaps;
	err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);
	assert(err == VK_SUCCESS);

	VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM; // TODO: find optimal

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = min(surfCaps.minImageCount + 1, surfCaps.maxImageCount);
	swapchainCreateInfo.imageFormat = colorFormat;
	swapchainCreateInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	swapchainCreateInfo.imageExtent = { width, height };
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = NULL;
#if 1
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // TODO: prefer VK_PRESENT_MODE_MAILBOX_KHR
#else
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
#endif
	swapchainCreateInfo.oldSwapchain = NULL;
	swapchainCreateInfo.clipped = true;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	VkSwapchainKHR swapChain = nullptr;
	// ERROR: [Swapchain] Code 4 : vkCreateSwapchainKHR() called with pCreateInfo->surface that was not returned by vkGetPhysicalDeviceSurfaceSupportKHR() for the device.
	err = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapChain);
	assert(err == VK_SUCCESS);

	uint32_t imageCount;
	err = vkGetSwapchainImagesKHR(device, swapChain, &imageCount, NULL);
	assert(err == VK_SUCCESS);
	assert(imageCount > 0);

	// Get the swap chain images
	VkImage *images = new VkImage[imageCount];
	VkImageView *imageViews = new VkImageView[imageCount];
	err = vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images);
	assert(err == VK_SUCCESS);

	for (uint32_t i = 0; i < imageCount; i++) {
		VkImageViewCreateInfo colorAttachmentView = {};
		colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = colorFormat;
		colorAttachmentView.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorAttachmentView.flags = 0;
		colorAttachmentView.image = images[i];

		err = vkCreateImageView(device, &colorAttachmentView, nullptr, &imageViews[i]);
		assert(err == VK_SUCCESS);
	}

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkSemaphore presentCompleteSemaphore;
	err = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore);
	assert(err == VK_SUCCESS);

	VkAttachmentDescription attachments[1];
	attachments[0].flags = 0;
	attachments[0].format = colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
//	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;

	VkRenderPassCreateInfo renderpassCreateInfo = {};
	renderpassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpassCreateInfo.attachmentCount = ARRAY_SIZE(attachments);
	renderpassCreateInfo.pAttachments = attachments;
	renderpassCreateInfo.subpassCount = 1;
	renderpassCreateInfo.pSubpasses = &subpass;

	VkRenderPass renderPass;
	err = vkCreateRenderPass(device, &renderpassCreateInfo, nullptr, &renderPass);
	assert(err == VK_SUCCESS);

	VkImageView framebufferAttachments[1];
	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = renderPass;
	framebufferCreateInfo.attachmentCount = ARRAY_SIZE(framebufferAttachments);
	framebufferCreateInfo.pAttachments = framebufferAttachments;
	framebufferCreateInfo.width = width;
	framebufferCreateInfo.height = height;
	framebufferCreateInfo.layers = 1;

	VkFramebuffer *framebuffers = new VkFramebuffer[imageCount];
	for (uint32_t i = 0; i < imageCount; i++) {
		framebufferAttachments[0] = imageViews[i];
		VkResult err = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffers[i]);
		assert(err == VK_SUCCESS);
	}

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool;
	err = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
	assert(err == VK_SUCCESS);

	VkCommandBufferAllocateInfo commandAllocInfo = { };
	commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandAllocInfo.commandPool = commandPool;
	commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandAllocInfo.commandBufferCount = imageCount;
	
	VkCommandBuffer *commandBuffers = new VkCommandBuffer[imageCount];
	err = vkAllocateCommandBuffers(device, &commandAllocInfo, commandBuffers);
	assert(err == VK_SUCCESS);


	// Main message loop:
	bool done = false;
	while (!done) {
		uint32_t currentSwapImage;
		err = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, NULL, &currentSwapImage);
		assert(err == VK_SUCCESS);

		VkCommandBuffer commandBuffer = commandBuffers[currentSwapImage];
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		assert(err == VK_SUCCESS);

		VkClearValue clearValue;
		clearValue = { { {
			float(rand()) / RAND_MAX,
			float(rand()) / RAND_MAX,
			float(rand()) / RAND_MAX,
			1.0f } } };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValue;
		renderPassBeginInfo.framebuffer = framebuffers[currentSwapImage];

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdEndRenderPass(commandBuffer);

		err = vkEndCommandBuffer(commandBuffer);
		assert(err == VK_SUCCESS);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &presentCompleteSemaphore;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Submit draw command buffer
		err = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(err == VK_SUCCESS);

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &currentSwapImage;
		err = vkQueuePresentKHR(graphicsQueue, &presentInfo);
		assert(err == VK_SUCCESS);

		err = vkQueueWaitIdle(graphicsQueue);
		assert(err == VK_SUCCESS);

		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			/* handle keys-events */
	
			if (WM_QUIT == msg.message ||
			    WM_CLOSE == msg.message ||
			    (WM_KEYDOWN == msg.message && VK_ESCAPE == LOWORD(msg.wParam)))
				done = true;
		}
	}

	return 0;
}

ATOM registerWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style         = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc   = WndProc;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hInstance     = hInstance;
	wcex.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)NULL;
	wcex.lpszMenuName  = NULL;
	wcex.lpszClassName = "vulkanwin";
	wcex.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	return RegisterClassEx(&wcex);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
