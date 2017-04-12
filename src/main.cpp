#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <list>
#include <map>

#include "vulkan.h"
#include "core/core.h"
#include "swapchain.h"
#include "shader.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace vulkan;

static std::vector<const char *> getRequiredInstanceExtensions()
{
	uint32_t requiredExtentionCount;
	auto tmp = glfwGetRequiredInstanceExtensions(&requiredExtentionCount);
	return std::vector<const char *>(tmp, tmp + requiredExtentionCount);
}

#include "scene/scene.h"

static VkDeviceSize alignSize(VkDeviceSize value, VkDeviceSize alignment)
{

	return ((value + alignment - 1) / alignment) * alignment;
}

static Texture2D generateXorTexture(int baseWidth, int baseHeight, int mipLevels, bool useStaging = true)
{
	Texture2D texture(VK_FORMAT_R8G8B8A8_UNORM, baseWidth, baseHeight, mipLevels, 1, useStaging);

	if (useStaging) {
		for (auto mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {

			auto mipWidth = TextureBase::mipSize(baseWidth, mipLevel),
			    mipHeight = TextureBase::mipSize(baseHeight, mipLevel);

			auto pitch = mipWidth * 4;
			auto size = pitch * mipHeight;

			auto stagingBuffer = new StagingBuffer(size);
			void *ptr = stagingBuffer->map(0, size);

			for (auto y = 0; y < mipHeight; ++y) {
				auto *row = static_cast<uint8_t *>(ptr) + pitch * y;
				for (auto x = 0; x < mipWidth; ++x) {
					uint8_t tmp = ((x ^ y) & 16) != 0 ? 0xFF : 0x00;
					row[x * 4 + 0] = 0x80 + (tmp >> 1);
					row[x * 4 + 1] = 0xFF - (tmp >> 1);
					row[x * 4 + 2] = 0x80 + (tmp >> 1);
					row[x * 4 + 3] = 0xFF;
				}
			}
			stagingBuffer->unmap();
			texture.uploadFromStagingBuffer(stagingBuffer, mipLevel);
			// TODO: delete staging buffer
		}
	}
	else {
		assert(mipLevels == 1);
		auto layout = texture.getSubresourceLayout(0, 0);
		void *ptr = texture.map(layout.offset, layout.size);

		for (auto y = 0; y < baseHeight; ++y) {
			auto *row = static_cast<uint8_t *>(ptr) + layout.rowPitch * y;
			for (auto x = 0; x < baseWidth; ++x) {
				uint8_t tmp = ((x ^ y) & 16) != 0 ? 0xFF : 0x00;
				row[x * 4 + 0] = 0x80 + (tmp >> 1);
				row[x * 4 + 1] = 0xFF - (tmp >> 1);
				row[x * 4 + 2] = 0x80 + (tmp >> 1);
				row[x * 4 + 3] = 0xFF;
			}
		}
		texture.unmap();
	}
	return texture;
}

static VkPipeline createGraphicsPipeline(int width, int height, VkPipelineLayout layout, VkRenderPass renderPass)
{
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
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState[1] = { { 0 } };
	pipelineColorBlendAttachmentState[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	pipelineColorBlendAttachmentState[0].blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.attachmentCount = ARRAY_SIZE(pipelineColorBlendAttachmentState);
	pipelineColorBlendStateCreateInfo.pAttachments = pipelineColorBlendAttachmentState;

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkViewport viewport = {
		0, 0,
		float(width), float(height),
		0.0f, 1.0f
	};

	VkRect2D scissor = {
		{ 0, 0 },
		{ width, height }
	};

	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.pViewports = &viewport;
	pipelineViewportStateCreateInfo.scissorCount = 1;
	pipelineViewportStateCreateInfo.pScissors = &scissor;

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
	pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkDynamicState dynamicStateEnables[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;
	pipelineDynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStateEnables);

	VkPipelineShaderStageCreateInfo shaderStages[] = { {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_VERTEX_BIT,
		loadShaderModule("shaders/triangle.vert.spv"),
		"main",
		NULL
	}, {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		loadShaderModule("shaders/triangle.frag.spv"),
		"main",
		NULL
	} };

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.layout = layout;
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
	auto err = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	assert(err == VK_SUCCESS);

	return pipeline;
}

static VkDescriptorSetLayout createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding> &layoutBindings)
{
	VkDescriptorSetLayoutCreateInfo desciptorSetLayoutCreateInfo = {};
	desciptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desciptorSetLayoutCreateInfo.bindingCount = layoutBindings.size();
	desciptorSetLayoutCreateInfo.pBindings = layoutBindings.data();

	VkDescriptorSetLayout descriptorSetLayout;
	auto err = vkCreateDescriptorSetLayout(device, &desciptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
	assert(err == VK_SUCCESS);

	return descriptorSetLayout;
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

	auto appName = "some excess demo";
	auto width = 1280, height = 720;
#ifdef NDEBUG
	auto fullscreen = true;
#else
	auto fullscreen = false;
#endif
	GLFWwindow *win = nullptr;

	try {
		if (!glfwInit())
			throw std::runtime_error("glfwInit failed!");

		if (!glfwVulkanSupported())
			throw std::runtime_error("no vulkan support!");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		win = glfwCreateWindow(width, height, appName, nullptr, nullptr);

		glfwSetKeyCallback(win, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			});


		auto enabledExtensions = getRequiredInstanceExtensions();
#ifndef NDEBUG
		enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

		instanceInit(appName, enabledExtensions);

		// Get number of available physical devices
		uint32_t physicalDeviceCount = 0;
		auto err = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
		assert(err == VK_SUCCESS);
		assert(physicalDeviceCount > 0);

		// Enumerate devices
		auto physicalDevices = new VkPhysicalDevice[physicalDeviceCount];
		err = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices);
		assert(err == VK_SUCCESS);
		assert(physicalDeviceCount > 0);

		auto physicalDevice = physicalDevices[0];

		for (uint32_t i = 0; i < physicalDeviceCount; ++i) {

			VkPhysicalDeviceProperties deviceProps;
			vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);

			VkPhysicalDeviceFeatures deviceFeats;
			vkGetPhysicalDeviceFeatures(physicalDevices[i], &deviceFeats);

			if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				auto physicalDevice = physicalDevices[i];
				break;
			}
		}
		delete[] physicalDevices;

		err = deviceInit(physicalDevice, [](VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueIndex) {
			return glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, queueIndex) == GLFW_TRUE;
		});
		if (err != VK_SUCCESS)
			throw std::runtime_error("init() failed!");

		VkSurfaceKHR surface;
		err = glfwCreateWindowSurface(instance, win, nullptr, &surface);
		if (err)
			throw std::runtime_error("glfwCreateWindowSurface failed!");

		auto swapChain = SwapChain(surface, width, height);

		std::vector<VkFormat> depthCandidates = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_D16_UNORM_S8_UINT,
		};

		auto depthFormat = findBestFormat(depthCandidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = depthFormat;
		imageCreateInfo.extent = { (uint32_t)width, (uint32_t)height, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkImage depthImage;
		err = vkCreateImage(device, &imageCreateInfo, nullptr, &depthImage);
		assert(err == VK_SUCCESS);

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, depthImage, &memoryRequirements);

		auto memoryTypeIndex = getMemoryTypeIndex(memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto deviceMemory = allocateDeviceMemory(memoryRequirements.size, memoryTypeIndex);

		err = vkBindImageMemory(device, depthImage, deviceMemory, 0);
		assert(err == VK_SUCCESS);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = depthImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = depthFormat;
		imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.subresourceRange.levelCount = 1;

		VkImageView depthImageView;
		err = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &depthImageView);
		assert(err == VK_SUCCESS);

		auto commandBuffer = allocateCommandBuffers(setupCommandPool, 1)[0];

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		assert(err == VK_SUCCESS);

		VkImageMemoryBarrier imageBarrier = {};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

		imageBarrier.image = depthImage;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		imageBarrier.dstQueueFamilyIndex = graphicsQueueIndex;
		imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		imageBarrier.srcQueueFamilyIndex = graphicsQueueIndex;
		imageBarrier.srcAccessMask = 0;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.levelCount = 1;
		imageBarrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier);

		err = vkEndCommandBuffer(commandBuffer);
		assert(err == VK_SUCCESS);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Submit draw command buffer
		err = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(err == VK_SUCCESS);

		VkAttachmentDescription attachments[2];
		attachments[0].flags = 0;
		attachments[0].format = swapChain.getSurfaceFormat().format;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		attachments[1].flags = 0;
		attachments[1].format = depthFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthStencilReference = {};
		depthStencilReference.attachment = 1;
		depthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pDepthStencilAttachment = &depthStencilReference;

		VkRenderPassCreateInfo renderpassCreateInfo = {};
		renderpassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderpassCreateInfo.attachmentCount = ARRAY_SIZE(attachments);
		renderpassCreateInfo.pAttachments = attachments;
		renderpassCreateInfo.subpassCount = 1;
		renderpassCreateInfo.pSubpasses = &subpass;

		VkRenderPass renderPass;
		err = vkCreateRenderPass(device, &renderpassCreateInfo, nullptr, &renderPass);
		assert(err == VK_SUCCESS);

		VkImageView framebufferAttachments[2];
		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = ARRAY_SIZE(framebufferAttachments);
		framebufferCreateInfo.pAttachments = framebufferAttachments;
		framebufferCreateInfo.width = width;
		framebufferCreateInfo.height = height;
		framebufferCreateInfo.layers = 1;

		framebufferAttachments[1] = depthImageView;

		auto imageViews = swapChain.getImageViews();
		auto framebuffers = new VkFramebuffer[imageViews.size()];
		for (auto i = 0u; i < imageViews.size(); i++) {
			framebufferAttachments[0] = imageViews[i];
			VkResult err = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffers[i]);
			assert(err == VK_SUCCESS);
		}

		VkCommandPool commandPool = createCommandPool(graphicsQueueIndex);

		Scene scene;

		glm::vec3 vertexPositions[] = {
			glm::vec3(-1.0f, 1.0f,-1.0f),
			glm::vec3(-1.0f, 1.0f, 1.0f),
			glm::vec3( 1.0f, 1.0f,-1.0f),
			glm::vec3( 1.0f, 1.0f, 1.0f),

			glm::vec3(-1.0f,-1.0f,-1.0f),
			glm::vec3( 1.0f,-1.0f,-1.0f),
			glm::vec3(-1.0f,-1.0f, 1.0f),
			glm::vec3( 1.0f,-1.0f, 1.0f),

			glm::vec3(-1.0f,-1.0f, 1.0f),
			glm::vec3( 1.0f,-1.0f, 1.0f),
			glm::vec3(-1.0f, 1.0f, 1.0f),
			glm::vec3( 1.0f, 1.0f, 1.0f),

			glm::vec3(-1.0f,-1.0f,-1.0f),
			glm::vec3(-1.0f, 1.0f,-1.0f),
			glm::vec3( 1.0f,-1.0f,-1.0f),
			glm::vec3( 1.0f, 1.0f,-1.0f),

			glm::vec3( 1.0f,-1.0f,-1.0f),
			glm::vec3( 1.0f, 1.0f,-1.0f),
			glm::vec3( 1.0f,-1.0f, 1.0f),
			glm::vec3( 1.0f, 1.0f, 1.0f),

			glm::vec3(-1.0f,-1.0f,-1.0f),
			glm::vec3(-1.0f,-1.0f, 1.0f),
			glm::vec3(-1.0f, 1.0f,-1.0f),
			glm::vec3(-1.0f, 1.0f, 1.0f),
		};

		uint16_t vertexIndices[] = {
			// front face
			0, 1, 2,
			2, 1, 3,

			// back face
			4, 5, 6,
			6, 5, 7,

			// top face
			8, 9, 10,
			10, 9, 11,

			// bottom face
			12, 13, 14,
			14, 13, 15,

			// left face
			16, 17, 18,
			18, 17, 19,

			// right face
			20, 21, 22,
			22, 21, 23,
		};

		Vertex v = { };
		v.position = glm::vec3(0, 0, 0);
		std::vector<Vertex> vertices;
		for (auto i = 0; i < ARRAY_SIZE(vertexPositions); ++i) {
			glm::vec3 pos = vertexPositions[i];
			v.position = pos;
			v.uv[0] = 0.5f + 0.5f * glm::vec2(pos.x, pos.y);
			vertices.push_back(v);
		}
		std::vector<uint32_t> indices;
		indices.push_back(0);
		indices.push_back(1);
		indices.push_back(2);
		auto mesh = Mesh(vertices, indices);
		auto material = Material();
		auto model = new Model(&mesh, &material);
		auto t1 = scene.createMatrixTransform();
		auto t2 = scene.createMatrixTransform(t1);
		auto obj1 = scene.createObject(model, t1);
		auto obj2 = scene.createObject(model, t2);

		// OK, let's prepare for rendering!

		auto baseWidth = 64, baseHeight = 64;
#if 1
		auto mipLevels = 2;
		auto texture = generateXorTexture(baseWidth, baseHeight, mipLevels, true);
#else
		auto mipLevels = 1;
		auto texture = generateXorTexture(baseWidth, baseHeight, mipLevels, false);
#endif

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
		samplerCreateInfo.maxLod = float(mipLevels);
		if (vulkan::enabledFeatures.samplerAnisotropy) {
			samplerCreateInfo.maxAnisotropy = 8;
			samplerCreateInfo.anisotropyEnable = VK_TRUE;
		}
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

		VkSampler textureSampler;
		err = vkCreateSampler(device, &samplerCreateInfo, nullptr, &textureSampler);
		assert(err == VK_SUCCESS);

		auto descriptorSetLayout = createDescriptorSetLayout({
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
			{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		});

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { };
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutCreateInfo.setLayoutCount = 1;

		VkPipelineLayout pipelineLayout;
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

		auto pipeline = createGraphicsPipeline(width, height, pipelineLayout, renderPass);

		auto descriptorPool = createDescriptorPool({
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		}, 1);

		auto uniformBufferSpacing = uint32_t(alignSize(sizeof(float) * 4 * 4, deviceProperties.limits.minUniformBufferOffsetAlignment));
		auto uniformBufferSize = VkDeviceSize(uniformBufferSpacing * scene.getTransforms().size());

		auto uniformBuffer = Buffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		auto descriptorSet = allocateDescriptorSet(descriptorPool, descriptorSetLayout);

		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = uniformBuffer.getBuffer();
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet writeDescriptorSet = {};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSet;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
		writeDescriptorSet.dstBinding = 0;
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		VkDescriptorImageInfo descriptorImageInfo = {};
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorImageInfo.imageView = texture.getImageView();
		descriptorImageInfo.sampler = textureSampler;

		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.pBufferInfo = nullptr;
		writeDescriptorSet.pImageInfo = &descriptorImageInfo;
		writeDescriptorSet.dstBinding = 1;
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		// Go make vertex buffer yo!
#if 1
		auto vertexStagingBuffer = new StagingBuffer(sizeof(vertexPositions));
		vertexStagingBuffer->uploadMemory(0, vertexPositions, sizeof(vertexPositions));

		auto vertexBuffer = Buffer(sizeof(vertexPositions), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vertexBuffer.uploadFromStagingBuffer(vertexStagingBuffer, 0, 0, sizeof(vertexPositions));
#else
		auto vertexBuffer = Buffer(sizeof(vertexPositions), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		vertexBuffer.uploadMemory(0, vertexPositions, sizeof(vertexPositions));
#endif

		auto indexBuffer = Buffer(sizeof(vertexIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		indexBuffer.uploadMemory(0, vertexIndices, sizeof(vertexIndices));

		auto backBufferSemaphore = createSemaphore(),
		     presentCompleteSemaphore = createSemaphore();

		auto commandBuffers = allocateCommandBuffers(commandPool, imageViews.size());

		auto commandBufferFences = new VkFence[imageViews.size()];
		for (auto i = 0u; i < imageViews.size(); ++i)
			commandBufferFences[i] = createFence(VK_FENCE_CREATE_SIGNALED_BIT);

		err = vkQueueWaitIdle(graphicsQueue);
		assert(err == VK_SUCCESS);

		auto startTime = glfwGetTime();
		while (!glfwWindowShouldClose(win)) {
			auto time = glfwGetTime() - startTime;

			auto currentSwapImage = swapChain.aquireNextImage(backBufferSemaphore);

			err = vkWaitForFences(device, 1, &commandBufferFences[currentSwapImage], VK_TRUE, UINT64_MAX);
			assert(err == VK_SUCCESS);

			err = vkResetFences(device, 1, &commandBufferFences[currentSwapImage]);
			assert(err == VK_SUCCESS);

			auto commandBuffer = commandBuffers[currentSwapImage];
			VkCommandBufferBeginInfo commandBufferBeginInfo = {};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
			assert(err == VK_SUCCESS);

			VkClearValue clearValues[2];
			clearValues[0].color = {
				0.5f,
				0.5f,
				0.5f,
				1.0f
			};
			clearValues[1].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			renderPassBeginInfo.clearValueCount = ARRAY_SIZE(clearValues);
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.framebuffer = framebuffers[currentSwapImage];

			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			setViewport(commandBuffer, 0, 0, float(width), float(height));
			setScissor(commandBuffer, 0, 0, width, height);

			auto th = float(time);

			// animate, yo
			t1->setLocalMatrix(glm::rotate(glm::mat4(1), th, glm::vec3(0, 0, 1)));
			t2->setLocalMatrix(glm::translate(glm::mat4(1), glm::vec3(cos(th), 1, 1)));

			auto viewPosition = glm::vec3(sin(th * 0.1f) * 10.0f, 0, cos(th * 0.1f) * 10.0f);
			auto viewMatrix = glm::lookAt(viewPosition, glm::vec3(0), glm::vec3(0, 1, 0));
			auto fov = 60.0f;
			auto aspect = float(width) / height;
			auto znear = 0.01f;
			auto zfar = 100.0f;
			auto projectionMatrix = glm::perspective(fov * float(M_PI / 180.0f), aspect, znear, zfar);
			auto viewProjectionMatrix = projectionMatrix * viewMatrix;

			auto offset = 0;
			std::map<const Transform*, int> offsetMap;
			auto transforms = scene.getTransforms();
			auto ptr = uniformBuffer.map(0, uniformBufferSpacing * transforms.size());
			for (auto transform : transforms) {
				auto modelMatrix = transform->getAbsoluteMatrix();
				auto modelViewProjectionMatrix = viewProjectionMatrix * modelMatrix;

				memcpy(static_cast<uint8_t *>(ptr) + offset, glm::value_ptr(modelViewProjectionMatrix), sizeof(modelViewProjectionMatrix));
				offsetMap[transform] = offset;
				offset += uniformBufferSpacing;
			}
			uniformBuffer.unmap();

			VkDeviceSize vertexBufferOffsets[1] = { 0 };
			VkBuffer vertexBuffers[1] = { vertexBuffer.getBuffer() };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vertexBufferOffsets);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT16);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			for (auto object : scene.getObjects()) {
				assert(offsetMap.count(object->getTransform()) > 0);

				auto offset = offsetMap[object->getTransform()];
				assert(offset <= uniformBufferSize - sizeof(float) * 4 * 4);
				uint32_t dynamicOffsets[] = { (uint32_t)offset };
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 1, dynamicOffsets);
				// vkCmdDraw(commandBuffer, ARRAY_SIZE(vertexPositions), 1, 0, 0);
				vkCmdDrawIndexed(commandBuffer, ARRAY_SIZE(vertexIndices), 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(commandBuffer);

			err = vkEndCommandBuffer(commandBuffer);
			assert(err == VK_SUCCESS);

			VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = &backBufferSemaphore;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &presentCompleteSemaphore;
			submitInfo.pWaitDstStageMask = &waitDstStageMask;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;

			// Submit draw command buffer
			err = vkQueueSubmit(graphicsQueue, 1, &submitInfo, commandBufferFences[currentSwapImage]);
			assert(err == VK_SUCCESS);

			swapChain.queuePresent(currentSwapImage, &presentCompleteSemaphore, 1);

			glfwPollEvents();
		}

		err = vkQueueWaitIdle(graphicsQueue);
		assert(err == VK_SUCCESS);

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
