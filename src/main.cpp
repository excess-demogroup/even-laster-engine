// vulkan-test.cpp : Defines the entry point for the application.
//

#include <algorithm>
#include <list>
#include <map>

#include "vulkan.h"
#include "core/core.h"
#include "core/memorymappedfile.h"
#include "swapchain.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace vulkan;

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

void uploadMemory(VkDeviceMemory deviceMemory, VkDeviceSize offset, void *data, VkDeviceSize size)
{
	void *mappedUniformMemory = nullptr;
	VkResult err = vkMapMemory(device, deviceMemory, offset, size, 0, &mappedUniformMemory);
	assert(err == VK_SUCCESS);

	memcpy(mappedUniformMemory, data, (size_t)size);

	vkUnmapMemory(device, deviceMemory);
}

std::vector<const char *> getRequiredInstanceExtensions()
{
	uint32_t requiredExtentionCount;
	const char **tmp = glfwGetRequiredInstanceExtensions(&requiredExtentionCount);
	return std::vector<const char *>(tmp, tmp + requiredExtentionCount);
}

#include "scene/scene.h"

VkDeviceSize alignSize(VkDeviceSize value, VkDeviceSize alignment)
{
	return ((value + alignment - 1) / alignment) * alignment;
}

VkFence createFence(VkFenceCreateFlags flags)
{
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = flags;

	VkFence ret;
	VkResult err = vkCreateFence(device, &fenceCreateInfo, nullptr, &ret);
	assert(err == VK_SUCCESS);
	return ret;
}

VkSemaphore createSemaphore()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkSemaphore ret;
	VkResult err = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &ret);
	assert(err == VK_SUCCESS);
	return ret;
}

Texture2D generateXorTexture(int baseWidth, int baseHeight, int mipLevels, bool useStaging = true)
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
				auto *row = (uint8_t *)ptr + pitch * y;
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
			auto *row = (uint8_t *)ptr + layout.rowPitch * y;
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
	auto width = 1280, height = 720;
#ifdef NDEBUG
	bool fullscreen = true;
#else
	bool fullscreen = false;
#endif
	GLFWwindow *win = nullptr;

	try {
		if (!glfwInit())
			throw std::runtime_error("glfwInit failed!");

		if (!glfwVulkanSupported())
			throw std::runtime_error("no vulkan support!");

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
			throw std::runtime_error("init() failed!");

		VkSurfaceKHR surface;
		err = glfwCreateWindowSurface(instance, win, nullptr, &surface);
		if (err)
			throw std::runtime_error("glfwCreateWindowSurface failed!");

		auto swapChain = SwapChain(surface, width, height);

		VkAttachmentDescription attachments[1];
		attachments[0].flags = 0;
		attachments[0].format = swapChain.getSurfaceFormat().format;
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

		auto imageViews = swapChain.getImageViews();
		auto framebuffers = new VkFramebuffer[imageViews.size()];
		for (auto i = 0u; i < imageViews.size(); i++) {
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

		Scene scene;

		glm::vec3 vertexPositions[] = {
			glm::vec3(1.0f, 1.0f, 0.0f),
			glm::vec3(-1.0f, 1.0f, 0.0f),
			glm::vec3(0.0f, -1.0f, 0.0f)
		};

		Vertex v = { };
		v.position = glm::vec3(0, 0, 0);
		std::vector<Vertex> vertices;
		for (auto i = 0; i < ARRAYSIZE(vertexPositions); ++i) {
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
		Texture2D texture = generateXorTexture(baseWidth, baseHeight, mipLevels, true);
#else
		auto mipLevels = 1;
		Texture2D texture = generateXorTexture(baseWidth, baseHeight, mipLevels, false);
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
		samplerCreateInfo.maxLod = (float)mipLevels;
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
		layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
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
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
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

		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		uint32_t uniformBufferSpacing = (uint32_t)alignSize(sizeof(float) * 4 * 4, physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);
		VkDeviceSize uniformBufferSize = uniformBufferSpacing * scene.getTransforms().size();

		auto uniformBuffer = Buffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = uniformBuffer.getBuffer();
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo descriptorImageInfo = {};
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorImageInfo.imageView = texture.getImageView();
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
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
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
#if 1
		auto vertexStagingBuffer = new StagingBuffer(sizeof(vertexPositions));
		uploadMemory(vertexStagingBuffer->getDeviceMemory(), 0, vertexPositions, sizeof(vertexPositions));

		auto vertexBuffer = Buffer(sizeof(vertexPositions), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vertexBuffer.uploadFromStagingBuffer(vertexStagingBuffer, 0, 0, sizeof(vertexPositions));
#else
		auto vertexBuffer = Buffer(sizeof(vertexPositions), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		uploadMemory(vertexBuffer.getDeviceMemory(), 0, vertexPositions, sizeof(vertexPositions));
#endif

		auto backBufferSemaphore = createSemaphore(),
		     presentCompleteSemaphore = createSemaphore();

		auto commandBuffers = allocateCommandBuffers(commandPool, imageViews.size());

		auto commandBufferFences = new VkFence[imageViews.size()];
		for (auto i = 0u; i < imageViews.size(); ++i)
			commandBufferFences[i] = createFence(VK_FENCE_CREATE_SIGNALED_BIT);

		err = vkQueueWaitIdle(graphicsQueue);
		assert(err == VK_SUCCESS);

		double startTime = glfwGetTime();
		while (!glfwWindowShouldClose(win)) {
			double time = glfwGetTime() - startTime;

			auto currentSwapImage = swapChain.aquireNextImage(backBufferSemaphore);

			err = vkWaitForFences(device, 1, &commandBufferFences[currentSwapImage], VK_TRUE, UINT64_MAX);
			assert(err == VK_SUCCESS);

			err = vkResetFences(device, 1, &commandBufferFences[currentSwapImage]);
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
			float a = float(height) / width;

			// animate, yo
			t1->setLocalMatrix(glm::rotate(glm::scale(glm::mat4(1), glm::vec3(a, 1, 1)), th, glm::vec3(0, 0, 1)));
			t2->setLocalMatrix(glm::translate(glm::mat4(1), glm::vec3(cos(th), 1, 1)));

			auto offset = 0;
			std::map<const Transform*, int> offsetMap;
			for each (auto transform in scene.getTransforms()) {
				auto modelMatrix = transform->getAbsoluteMatrix();
				uploadMemory(uniformBuffer.getDeviceMemory(), offset, glm::value_ptr(modelMatrix), sizeof(modelMatrix));
				offsetMap[transform] = offset;
				offset += uniformBufferSpacing;
			}

			VkDeviceSize vertexBufferOffsets[1] = { 0 };
			VkBuffer vertexBuffers[1] = { vertexBuffer.getBuffer() };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vertexBufferOffsets);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

			for each (auto object in scene.getObjects()) {
				assert(offsetMap.count(object->getTransform()) > 0);

				auto offset = offsetMap[object->getTransform()];
				assert(offset <= uniformBufferSize - sizeof(float) * 4 * 4);
				uint32_t dynamicOffsets[] = { offset };
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 1, dynamicOffsets);
				vkCmdDraw(commandBuffer, 3, 1, 0, 0);
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
