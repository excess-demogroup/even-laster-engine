// vulkan-test.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "vulkan.h"
#include "memorymappedfile.h"
#include <GLFW/glfw3.h>

using namespace vulkan;

static VkSurfaceKHR surface;

VkShaderModule loadShaderModule(const char *path, VkDevice device, VkShaderStageFlagBits stage)
{
	MemoryMappedFile shaderCode(path);
	assert(shaderCode.getSize() > 0);

	VkShaderModuleCreateInfo moduleCreateInfo = {};
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.codeSize = shaderCode.getSize();
	moduleCreateInfo.pCode = (uint32_t *)shaderCode.getData();

	VkShaderModule shaderModule;
	VkResult err = vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule);
	assert(!err);

	return shaderModule;
}

VkPipelineShaderStageCreateInfo loadShader(const char * fileName, VkDevice device, VkShaderStageFlagBits stage, const char *name = "main")
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.module = loadShaderModule(fileName, device, stage);
	shaderStage.pName = name;
	return shaderStage;
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkBuffer *buffer)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usageFlags;
	VkResult err = vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer);
	assert(err == VK_SUCCESS);
}

VkDeviceMemory allocateDeviceMemory(const VkMemoryRequirements &memoryRequirements, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = UINT32_MAX;

	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
		if (((memoryRequirements.memoryTypeBits >> i) & 1) == 1) {
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
				memoryAllocateInfo.memoryTypeIndex = i;
				break;
			}
		}
	}
	assert(memoryAllocateInfo.memoryTypeIndex != UINT32_MAX);

	VkDeviceMemory deviceMemory;
	VkResult err = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceMemory);
	assert(err == VK_SUCCESS);

	return deviceMemory;
}

VkDeviceMemory allocateBufferDeviceMemory(VkBuffer buffer, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

	auto deviceMemory = allocateDeviceMemory(memoryRequirements, propertyFlags);

	VkResult err = vkBindBufferMemory(device, buffer, deviceMemory, 0);
	assert(!err);

	return deviceMemory;
}

VkDeviceMemory allocateImageDeviceMemory(VkImage image, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, image, &memoryRequirements);

	auto deviceMemory = allocateDeviceMemory(memoryRequirements, propertyFlags);

	VkResult err = vkBindImageMemory(device, image, deviceMemory, 0);
	assert(!err);

	return deviceMemory;
}

void uploadMemory(VkDeviceMemory deviceMemory, size_t offset, void *data, size_t size)
{
	void *mappedUniformMemory = nullptr;
	VkResult err = vkMapMemory(device, deviceMemory, offset, size, 0, &mappedUniformMemory);
	assert(err == VK_SUCCESS);

	memcpy(mappedUniformMemory, data, size);

	vkUnmapMemory(device, deviceMemory);
}

VkCommandBuffer *allocateCommandBuffers(VkCommandPool commandPool, int commandBufferCount)
{
	VkCommandBufferAllocateInfo commandAllocInfo = {};
	commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandAllocInfo.commandPool = commandPool;
	commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandAllocInfo.commandBufferCount = commandBufferCount;

	auto commandBuffers = new VkCommandBuffer[commandBufferCount];
	VkResult err = vkAllocateCommandBuffers(device, &commandAllocInfo, commandBuffers);
	assert(err == VK_SUCCESS);

	return commandBuffers;
}

std::vector<const char *> getRequiredInstanceExtensions()
{
	uint32_t requiredExtentionCount;
	const char **tmp = glfwGetRequiredInstanceExtensions(&requiredExtentionCount);
	return std::vector<const char *>(tmp, tmp + requiredExtentionCount);
}

#ifdef WIN32
int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR lpCmdLine,
                     _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);
#else
int main(int argc, char *argv[])
{
#endif

	const char *appName = "some excess demo";
	int width = 1280, height = 720;
#ifdef NDEBUG
	bool fullscreen = true;
#else
	bool fullscreen = false;
#endif
	GLFWwindow *win = nullptr;

	try {
		if (!glfwInit())
			throw std::exception("glfwInit failed!");

		if (!glfwVulkanSupported())
			throw std::exception("no vulkan support!");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		win = glfwCreateWindow(width, height, appName, nullptr, nullptr);

		glfwSetKeyCallback(win, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			});


		auto enabledExtensions = getRequiredInstanceExtensions();
#ifndef NDEBUG
		enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

		VkResult err = init(appName, enabledExtensions);
		if (err != VK_SUCCESS)
			throw std::exception("init() failed!");

		err = glfwCreateWindowSurface(instance, win, nullptr, &surface);
		if (err)
			throw std::exception("glfwCreateWindowSurface failed!");

		VkBool32 surfaceSupported = VK_FALSE;
		err = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueIndex, surface, &surfaceSupported);
		assert(err == VK_SUCCESS);
		assert(surfaceSupported == VK_TRUE);

		VkSurfaceCapabilitiesKHR surfCaps;
		err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);
		assert(err == VK_SUCCESS);

		uint32_t surfaceFormatCount = 0;
		err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
		assert(err == VK_SUCCESS);
		assert(surfaceFormatCount > 0);
		auto surfaceFormats = new VkSurfaceFormatKHR[surfaceFormatCount];
		err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats);
		assert(err == VK_SUCCESS);

		// find sRGB color format
		VkSurfaceFormatKHR surfaceFormat = surfaceFormats[0];
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

		VkSwapchainKHR swapChain = VK_NULL_HANDLE;
		err = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapChain);
		assert(err == VK_SUCCESS);

		uint32_t imageCount;
		err = vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		assert(err == VK_SUCCESS);
		assert(imageCount > 0);

		// Get the swap chain images
		auto images = new VkImage[imageCount];
		auto imageViews = new VkImageView[imageCount];
		err = vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images);
		assert(err == VK_SUCCESS);

		for (uint32_t i = 0; i < imageCount; i++) {
			VkImageViewCreateInfo colorAttachmentView = {};
			colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			colorAttachmentView.format = surfaceFormat.format;
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
			colorAttachmentView.image = images[i];

			err = vkCreateImageView(device, &colorAttachmentView, nullptr, &imageViews[i]);
			assert(err == VK_SUCCESS);
		}

		VkAttachmentDescription attachments[1];
		attachments[0].flags = 0;
		attachments[0].format = surfaceFormat.format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

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

		auto framebuffers = new VkFramebuffer[imageCount];
		for (uint32_t i = 0; i < imageCount; i++) {
			framebufferAttachments[0] = imageViews[i];
			VkResult err = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffers[i]);
			assert(err == VK_SUCCESS);
		}

		VkCommandPoolCreateInfo commandPoolCreateInfo = {};
		commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolCreateInfo.queueFamilyIndex = graphicsQueueIndex;
		commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VkCommandPool commandPool;
		err = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
		assert(err == VK_SUCCESS);

		auto commandBuffers = allocateCommandBuffers(commandPool, imageCount);

		// OK, let's prepare for rendering!

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR; // TODO: VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.flags = 0;
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.extent = { 64, 64, 1 };
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

		VkImage textureImage;
		err = vkCreateImage(device, &imageCreateInfo, nullptr, &textureImage);
		assert(err == VK_SUCCESS);

		VkDeviceMemory textureDeviceMemory = allocateImageDeviceMemory(textureImage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT); // TODO: get rid of VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT

		{
			VkImageSubresource subRes = {};
			subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			VkSubresourceLayout subresourceLayout;
			vkGetImageSubresourceLayout(device, textureImage, &subRes, &subresourceLayout);

			void *mappedMemory;
			err = vkMapMemory(device, textureDeviceMemory, subresourceLayout.offset, subresourceLayout.size, 0, &mappedMemory);
			assert(err == VK_SUCCESS);
			for (int y = 0; y < 64; ++y) {
				uint8_t *row = (uint8_t *)mappedMemory + subresourceLayout.rowPitch * y;
				for (int x = 0; x < 64; ++x) {
					uint8_t tmp = ((x ^ y) & 16) != 0 ? 0xFF : 0x00;
					row[x * 4 + 0] = 0x80 + (tmp >> 1);
					row[x * 4 + 1] = 0xFF - (tmp >> 1);
					row[x * 4 + 2] = 0x80 + (tmp >> 1);
					row[x * 4 + 3] = 0xFF;
				}
			}
			vkUnmapMemory(device, textureDeviceMemory);
		}

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = textureImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = imageCreateInfo.format;
		imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.subresourceRange.levelCount = 1;

		VkImageView textureImageView;
		err = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &textureImageView);
		assert(err == VK_SUCCESS);

		VkSamplerCreateInfo samplerCreateInfo = {};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
		samplerCreateInfo.minLod = 0.0f;
		samplerCreateInfo.maxLod = 0.0f;
		samplerCreateInfo.maxAnisotropy = 8;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		VkSampler textureSampler;
		err = vkCreateSampler(device, &samplerCreateInfo, nullptr, &textureSampler);
		assert(err == VK_SUCCESS);

		VkPipelineShaderStageCreateInfo shaderStages[] = {
			loadShader("shaders/triangle.vert.spv", device, VK_SHADER_STAGE_VERTEX_BIT),
			loadShader("shaders/triangle.frag.spv", device, VK_SHADER_STAGE_FRAGMENT_BIT)
		};

		VkVertexInputBindingDescription vertexInputBindingDesc[1];
		vertexInputBindingDesc[0].binding = 0;
		vertexInputBindingDesc[0].stride = sizeof(float) * 3;
		vertexInputBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertexInputAttributeDescription[1];
		vertexInputAttributeDescription[0].binding = 0;
		vertexInputAttributeDescription[0].location = 0;
		vertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributeDescription[0].offset = 0;

		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = ARRAY_SIZE(vertexInputBindingDesc);
		pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindingDesc;
		pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = ARRAY_SIZE(vertexInputAttributeDescription);
		pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescription;

		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
		pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
		pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_TRUE;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState[1] = { { 0 } };
		pipelineColorBlendAttachmentState[0].colorWriteMask = 0xf;
		pipelineColorBlendAttachmentState[0].blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
		pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		pipelineColorBlendStateCreateInfo.attachmentCount = ARRAY_SIZE(pipelineColorBlendAttachmentState);
		pipelineColorBlendStateCreateInfo.pAttachments = pipelineColorBlendAttachmentState;

		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
		pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkViewport viewport = {
			0, 0, (float)width, (float)height, 0.0f, 1.0f
		};

		VkRect2D scissor = {
			{0, 0},
			{width, height}
		};

		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
		pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		pipelineViewportStateCreateInfo.viewportCount = 1;
		pipelineViewportStateCreateInfo.pViewports = &viewport;
		pipelineViewportStateCreateInfo.scissorCount = 1;
		pipelineViewportStateCreateInfo.pScissors = &scissor;

		VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
		pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
		pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;

		VkDescriptorSetLayoutBinding layoutBindings[2] = {};
		layoutBindings[0].binding = 0;
		layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		layoutBindings[0].descriptorCount = 1;
		layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		layoutBindings[1].binding = 1;
		layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layoutBindings[1].descriptorCount = 1;
		layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = { };
		descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCreateInfo.bindingCount = ARRAY_SIZE(layoutBindings);
		descSetLayoutCreateInfo.pBindings = layoutBindings;

		VkDescriptorSetLayout descriptorSetLayout;
		err = vkCreateDescriptorSetLayout(device, &descSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
		assert(err == VK_SUCCESS);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { };
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutCreateInfo.setLayoutCount = 1;

		VkPipelineLayout pipelineLayout;
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

		VkDynamicState dynamicStateEnables[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
		pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;
		pipelineDynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStateEnables);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
		pipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
		pipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
		pipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
		pipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
		pipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
		pipelineCreateInfo.stageCount = ARRAY_SIZE(shaderStages);
		pipelineCreateInfo.pStages = shaderStages;

		VkPipeline pipeline;
		err = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
		assert(err == VK_SUCCESS);

		VkDescriptorPoolSize descriptorPoolSizes[] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		};

		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.poolSizeCount = ARRAY_SIZE(descriptorPoolSizes);
		descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;
		descriptorPoolCreateInfo.maxSets = 1;

		VkDescriptorPool descriptorPool;
		err = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
		assert(err == VK_SUCCESS);

		VkBuffer uniformBuffer;
		VkDeviceSize uniformBufferSize = sizeof(float) * 4 * 4;
		createBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &uniformBuffer);

		VkDeviceMemory uniformDeviceMemory = allocateBufferDeviceMemory(uniformBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = uniformBuffer;
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo descriptorImageInfo = {};
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorImageInfo.imageView = textureImageView;
		descriptorImageInfo.sampler = textureSampler;

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

		VkDescriptorSet descriptorSet;
		err = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
		assert(err == VK_SUCCESS);

		VkWriteDescriptorSet writeDescriptorSet = {};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
		writeDescriptorSet.dstBinding = 0;
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.pBufferInfo = nullptr;
		writeDescriptorSet.pImageInfo = &descriptorImageInfo;
		writeDescriptorSet.dstBinding = 1;
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		// Go make vertex buffer yo!

		float vertexData[] = {
			 1.0f,  1.0f, 0.0f,
			-1.0f,  1.0f, 0.0f,
			 0.0f, -1.0f, 0.0f
		};

		VkBuffer vertexBuffer;
		createBuffer(sizeof(vertexData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vertexBuffer);

		VkDeviceMemory vertexDeviceMemory = allocateBufferDeviceMemory(vertexBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		uploadMemory(vertexDeviceMemory, 0, vertexData, sizeof(vertexData));

		double startTime = glfwGetTime();
		while (!glfwWindowShouldClose(win)) {
			double time = glfwGetTime() - startTime;

			VkSemaphoreCreateInfo semaphoreCreateInfo = {};
			semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VkSemaphore presentCompleteSemaphore;
			err = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore);
			assert(err == VK_SUCCESS);

			uint32_t currentSwapImage;
			err = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, VK_NULL_HANDLE, &currentSwapImage);
			assert(err == VK_SUCCESS);

			VkCommandBuffer commandBuffer = commandBuffers[currentSwapImage];
			VkCommandBufferBeginInfo commandBufferBeginInfo = {};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			assert(err == VK_SUCCESS);

			VkClearValue clearValue = { { {
				0.5f,
				0.5f,
				0.5f,
				1.0f
			} } };

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

			VkViewport viewport = {};
			viewport.height = (float)height;
			viewport.width = (float)width;
			viewport.minDepth = (float)0.0f;
			viewport.maxDepth = (float)1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			VkRect2D scissor = {};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			float th = (float)time;
			float a = float(width) / height;
			float uniformData[] = {
				cos(th) / a, -sin(th), 0.0f, 0.0f,
				sin(th) / a,  cos(th), 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f,
			};

			uploadMemory(uniformDeviceMemory, 0, uniformData, sizeof(uniformData));

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

			vkCmdDraw(commandBuffer, 3, 1, 0, 0);

			vkCmdEndRenderPass(commandBuffer);

			err = vkEndCommandBuffer(commandBuffer);
			assert(err == VK_SUCCESS);

			VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &presentCompleteSemaphore;
			submitInfo.pWaitDstStageMask = &waitDstStageMask;
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

			glfwPollEvents();
		}
	} catch (const std::exception &e) {
		if (win != nullptr)
			glfwDestroyWindow(win);

#ifdef WIN32
		MessageBox(nullptr, e.what(), nullptr, MB_OK);
#else
		fprintf(stderr, "FATAIL ERROR: %s\n", e.what());
#endif
	}

	glfwTerminate();
	return 0;
}
