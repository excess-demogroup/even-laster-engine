#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <list>
#include <map>
#include <stdexcept>

#include "vulkan.h"
#include "core/core.h"
#include "swapchain.h"
#include "shader.h"
#include "scene/import-texture.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace vulkan;

using std::vector;
using std::map;
using std::exception;
using std::runtime_error;

static vector<const char *> getRequiredInstanceExtensions()
{
	uint32_t requiredExtentionCount;
	auto tmp = glfwGetRequiredInstanceExtensions(&requiredExtentionCount);
	return vector<const char *>(tmp, tmp + requiredExtentionCount);
}

#include "scene/scene.h"
#include "scene/rendertarget.h"

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
	} else {
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

static VkPipeline createGraphicsPipeline(VkPipelineLayout layout, VkRenderPass renderPass, const VkPipelineVertexInputStateCreateInfo &pipelineVertexInputStateCreateInfo)
{
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

	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.pViewports = nullptr;
	pipelineViewportStateCreateInfo.scissorCount = 1;
	pipelineViewportStateCreateInfo.pScissors = nullptr;

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
		loadShaderModule("data/shaders/triangle.vert.spv"),
		"main",
		NULL
	}, {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		loadShaderModule("data/shaders/triangle.frag.spv"),
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

static VkPipeline createComputePipeline(VkPipelineLayout layout, VkShaderModule shaderModule, const char *name = "main")
{
	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computePipelineCreateInfo.stage.module = shaderModule;
	computePipelineCreateInfo.stage.pName = name;
	computePipelineCreateInfo.layout = layout;

	VkPipeline computePipeline;
	auto err = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline);
	assert(err == VK_SUCCESS);
	return computePipeline;
}

static VkDescriptorSetLayout createDescriptorSetLayout(const vector<VkDescriptorSetLayoutBinding> &layoutBindings)
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

namespace CubeData
{
	glm::vec3 vertexPositions[] = {
		glm::vec3(-1.0f, 1.0f, -1.0f),
		glm::vec3(-1.0f, 1.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, -1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),

		glm::vec3(-1.0f, -1.0f, -1.0f),
		glm::vec3(1.0f, -1.0f, -1.0f),
		glm::vec3(-1.0f, -1.0f, 1.0f),
		glm::vec3(1.0f, -1.0f, 1.0f),

		glm::vec3(-1.0f, -1.0f, 1.0f),
		glm::vec3(1.0f, -1.0f, 1.0f),
		glm::vec3(-1.0f, 1.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),

		glm::vec3(-1.0f, -1.0f, -1.0f),
		glm::vec3(-1.0f, 1.0f, -1.0f),
		glm::vec3(1.0f, -1.0f, -1.0f),
		glm::vec3(1.0f, 1.0f, -1.0f),

		glm::vec3(1.0f, -1.0f, -1.0f),
		glm::vec3(1.0f, 1.0f, -1.0f),
		glm::vec3(1.0f, -1.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),

		glm::vec3(-1.0f, -1.0f, -1.0f),
		glm::vec3(-1.0f, -1.0f, 1.0f),
		glm::vec3(-1.0f, 1.0f, -1.0f),
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
	auto fullscreen = false;
	GLFWwindow *win = nullptr;

	try {
		if (!glfwInit())
			throw runtime_error("glfwInit failed!");

		if (!glfwVulkanSupported())
			throw runtime_error("no vulkan support!");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		win = glfwCreateWindow(width, height, appName, fullscreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);

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

			if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				physicalDevice = physicalDevices[i];
				break;
			}
		}
		delete[] physicalDevices;

		deviceInit(physicalDevice, [](VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueIndex) {
			return glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, queueIndex) == GLFW_TRUE;
		});

		VkSurfaceKHR surface;
		err = glfwCreateWindowSurface(instance, win, nullptr, &surface);
		if (err)
			throw runtime_error("glfwCreateWindowSurface failed!");

		auto swapChain = SwapChain(surface, width, height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		vector<VkFormat> depthCandidates = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D16_UNORM,
		};

		auto depthFormat = findBestFormat(depthCandidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		DepthRenderTarget depthRenderTarget(depthFormat, width, height);

		vector<VkFormat> renderTargetCandidates = {
			VK_FORMAT_R16G16B16A16_SFLOAT
		};
		auto renderTargetFormat = findBestFormat(renderTargetCandidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
		ColorRenderTarget colorRenderTarget(renderTargetFormat, width, height, VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

		auto computeTargetFormat = findBestFormat(renderTargetCandidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
		ColorRenderTarget computeRenderTarget(computeTargetFormat, width, height, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		VkAttachmentDescription attachments[2];
		attachments[0].flags = 0;
		attachments[0].format = depthFormat;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		attachments[1].flags = 0;
		attachments[1].format = renderTargetFormat;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference depthStencilReference = {};
		depthStencilReference.attachment = 0;
		depthStencilReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 1;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

		framebufferAttachments[0] = depthRenderTarget.getImageView();
		framebufferAttachments[1] = colorRenderTarget.getImageView();

		VkFramebuffer framebuffer;
		err = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer);
		assert(err == VK_SUCCESS);

		auto imageViews = swapChain.getImageViews();
		auto images = swapChain.getImages();

		Scene scene;

		Vertex v = {};
		v.position = glm::vec3(0, 0, 0);
		vector<Vertex> vertices;
		for (auto i = 0u; i < ARRAY_SIZE(CubeData::vertexPositions); ++i) {
			glm::vec3 pos = CubeData::vertexPositions[i];
			v.position = pos;
			v.uv[0] = 0.5f + 0.5f * glm::vec2(pos.x, pos.y);
			vertices.push_back(v);
		}
		vector<uint32_t> indices;
		indices.push_back(0);
		indices.push_back(1);
		indices.push_back(2);
		auto mesh = Mesh(vertices, indices);
		auto material = Material();
		auto model = new Model(&mesh, &material);
		auto t1 = scene.createMatrixTransform();
		auto t2 = scene.createMatrixTransform(t1);
		scene.createObject(model, t1);
		scene.createObject(model, t2);

		// OK, let's prepare for rendering!

		auto baseWidth = 64, baseHeight = 64;
#if 1
		auto texture = importTexture("assets/excess-logo.png", TextureImportFlags::GENERATE_MIPMAPS);
#elif 0
		auto mipLevels = 2;
		auto texture = generateXorTexture(baseWidth, baseHeight, mipLevels, true);
#else
		auto mipLevels = 1;
		auto texture = generateXorTexture(baseWidth, baseHeight, mipLevels, false);
#endif


		auto descriptorSetLayout = createDescriptorSetLayout({
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT },
			{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		});

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
		pipelineLayoutCreateInfo.setLayoutCount = 1;

		VkPipelineLayout pipelineLayout;
		vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

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

		auto pipeline = createGraphicsPipeline(pipelineLayout, renderPass, pipelineVertexInputStateCreateInfo);

		auto descriptorPool = createDescriptorPool({
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		}, 1);

		struct {
			glm::mat4 modelViewProjectionMatrix;
		} perObjectUniforms;
		auto uniformSize = sizeof(perObjectUniforms);
		auto uniformBufferSpacing = uint32_t(alignSize(uniformSize, deviceProperties.limits.minUniformBufferOffsetAlignment));
		auto uniformBufferSize = VkDeviceSize(uniformBufferSpacing * scene.getTransforms().size());

		auto uniformBuffer = Buffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		auto descriptorSet = allocateDescriptorSet(descriptorPool, descriptorSetLayout);

		VkDescriptorBufferInfo descriptorBufferInfo = uniformBuffer.getDescriptorBufferInfo();

		VkWriteDescriptorSet writeDescriptorSets[2] = {};
		writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[0].dstSet = descriptorSet;
		writeDescriptorSets[0].descriptorCount = 1;
		writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		writeDescriptorSets[0].pBufferInfo = &descriptorBufferInfo;
		writeDescriptorSets[0].dstBinding = 0;

		VkSampler textureSampler = createSampler(float(texture.getMipLevels()), true, true);

		VkDescriptorImageInfo descriptorImageInfo = texture.getDescriptorImageInfo(textureSampler);
		writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[1].dstSet = descriptorSet;
		writeDescriptorSets[1].descriptorCount = 1;
		writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSets[1].pBufferInfo = nullptr;
		writeDescriptorSets[1].pImageInfo = &descriptorImageInfo;
		writeDescriptorSets[1].dstBinding = 1;
		vkUpdateDescriptorSets(device, ARRAY_SIZE(writeDescriptorSets), writeDescriptorSets, 0, nullptr);

		// Go make vertex buffer yo!
#if 1
		auto vertexStagingBuffer = new StagingBuffer(sizeof(CubeData::vertexPositions));
		vertexStagingBuffer->uploadMemory(0, CubeData::vertexPositions, sizeof(CubeData::vertexPositions));

		auto vertexBuffer = Buffer(sizeof(CubeData::vertexPositions), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vertexBuffer.uploadFromStagingBuffer(vertexStagingBuffer, 0, 0, sizeof(CubeData::vertexPositions));
#else
		auto vertexBuffer = Buffer(sizeof(CubeData::vertexPositions), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		vertexBuffer.uploadMemory(0, CubeData::vertexPositions, sizeof(CubeData::vertexPositions));
#endif

		auto indexBuffer = Buffer(sizeof(CubeData::vertexIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		indexBuffer.uploadMemory(0, CubeData::vertexIndices, sizeof(CubeData::vertexIndices));

		VkDescriptorSetLayoutBinding computeDescriptorSetLayoutBindings[] = {
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0 },
			{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
		};

		VkDescriptorSetLayoutCreateInfo computeDescSetLayoutCreateInfo = {};
		computeDescSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		computeDescSetLayoutCreateInfo.bindingCount = ARRAY_SIZE(computeDescriptorSetLayoutBindings);
		computeDescSetLayoutCreateInfo.pBindings = computeDescriptorSetLayoutBindings;

		VkDescriptorSetLayout computeDescriptorSetLayout;
		err = vkCreateDescriptorSetLayout(device, &computeDescSetLayoutCreateInfo, nullptr, &computeDescriptorSetLayout);
		assert(err == VK_SUCCESS);

		VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo = {};
		computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		computePipelineLayoutCreateInfo.pSetLayouts = &computeDescriptorSetLayout;
		computePipelineLayoutCreateInfo.setLayoutCount = 1;

		VkPipelineLayout computePipelineLayout;
		vkCreatePipelineLayout(device, &computePipelineLayoutCreateInfo, nullptr, &computePipelineLayout);

		VkPipeline computePipeline = createComputePipeline(computePipelineLayout, loadShaderModule("data/shaders/postprocess.comp.spv"));

		auto computeDescriptorPool = createDescriptorPool({
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageViews.size() },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageViews.size() },
		}, imageViews.size());

		auto computeDescriptorSetLayouts = vector<VkDescriptorSetLayout>(imageViews.size(), computeDescriptorSetLayout);
		VkDescriptorSetAllocateInfo computeDescriptorSetAllocateInfo = {};
		computeDescriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		computeDescriptorSetAllocateInfo.descriptorPool = computeDescriptorPool;
		computeDescriptorSetAllocateInfo.descriptorSetCount = computeDescriptorSetLayouts.size();
		computeDescriptorSetAllocateInfo.pSetLayouts = computeDescriptorSetLayouts.data();

		auto computeDescriptorSets = new VkDescriptorSet[imageViews.size()];

		err = vkAllocateDescriptorSets(device, &computeDescriptorSetAllocateInfo, computeDescriptorSets);
		assert(err == VK_SUCCESS);

		auto backBufferSemaphore = createSemaphore(),
		     presentCompleteSemaphore = createSemaphore();

		VkCommandPool commandPool = createCommandPool(graphicsQueueIndex);
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
			clearValues[0].depthStencil = { 1.0f, 0 };
			clearValues[1].color = {
				0.5f,
				0.5f,
				0.5f,
				1.0f
			};

			VkRenderPassBeginInfo renderPassBeginInfo = {};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassBeginInfo.renderPass = renderPass;
			renderPassBeginInfo.renderArea.offset.x = 0;
			renderPassBeginInfo.renderArea.offset.y = 0;
			renderPassBeginInfo.renderArea.extent.width = width;
			renderPassBeginInfo.renderArea.extent.height = height;
			renderPassBeginInfo.clearValueCount = ARRAY_SIZE(clearValues);
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.framebuffer = framebuffer;

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

			auto offset = 0u;
			map<const Transform*, unsigned int> offsetMap;
			auto transforms = scene.getTransforms();
			auto ptr = uniformBuffer.map(0, uniformBufferSpacing * transforms.size());
			for (auto transform : transforms) {
				auto modelMatrix = transform->getAbsoluteMatrix();
				auto modelViewProjectionMatrix = viewProjectionMatrix * modelMatrix;
				perObjectUniforms.modelViewProjectionMatrix = modelViewProjectionMatrix;
				memcpy(static_cast<uint8_t *>(ptr) + offset, &perObjectUniforms, sizeof(perObjectUniforms));
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
				assert(offset <= uniformBufferSize - uniformSize);
				uint32_t dynamicOffsets[] = { (uint32_t)offset };
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 1, dynamicOffsets);
				// vkCmdDraw(commandBuffer, ARRAY_SIZE(vertexPositions), 1, 0, 0);
				vkCmdDrawIndexed(commandBuffer, ARRAY_SIZE(CubeData::vertexIndices), 1, 0, 0, 0);
			}

			vkCmdEndRenderPass(commandBuffer);

			VkDescriptorImageInfo computeDescriptorImageInfo = {};
			computeDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			computeDescriptorImageInfo.imageView = computeRenderTarget.getImageView();

			{
				VkWriteDescriptorSet writeDescriptorSets[2] = {};
				writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[0].dstSet = computeDescriptorSets[currentSwapImage];
				writeDescriptorSets[0].dstBinding = 0;
				writeDescriptorSets[0].descriptorCount = 1;
				writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				writeDescriptorSets[0].pImageInfo = &computeDescriptorImageInfo;

				VkDescriptorImageInfo descriptorImageInfo = {};
				descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				descriptorImageInfo.imageView = colorRenderTarget.getImageView();
				descriptorImageInfo.sampler = textureSampler;

				writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSets[1].dstSet = computeDescriptorSets[currentSwapImage];
				writeDescriptorSets[1].dstBinding = 1;
				writeDescriptorSets[1].descriptorCount = 1;
				writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSets[1].pImageInfo = &descriptorImageInfo;

				vkUpdateDescriptorSets(device, ARRAY_SIZE(writeDescriptorSets), writeDescriptorSets, 0, nullptr);
			}


			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSets[currentSwapImage], 0, nullptr);

			imageBarrier(
				commandBuffer,
				computeRenderTarget.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			vkCmdDispatch(commandBuffer, width / 16, height / 16, 1);

			imageBarrier(
				commandBuffer,
				computeRenderTarget.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			imageBarrier(
				commandBuffer,
				images[currentSwapImage],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			VkImageBlit imageBlit = { };
			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.layerCount = 1;
			imageBlit.srcSubresource.mipLevel = 0;
			imageBlit.srcOffsets[1].x = width;
			imageBlit.srcOffsets[1].y = height;
			imageBlit.srcOffsets[1].z = 1;
			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.layerCount = 1;
			imageBlit.dstSubresource.mipLevel = 0;
			imageBlit.dstOffsets[1].x = width;
			imageBlit.dstOffsets[1].y = height;
			imageBlit.dstOffsets[1].z = 1;

			vkCmdBlitImage(commandBuffer,
				computeRenderTarget.getImage(),
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				images[currentSwapImage],
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &imageBlit,
				VK_FILTER_NEAREST);

			imageBarrier(
				commandBuffer,
				images[currentSwapImage],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_ACCESS_TRANSFER_WRITE_BIT, 0,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

			err = vkEndCommandBuffer(commandBuffer);
			assert(err == VK_SUCCESS);

			VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_BIND_POINT_COMPUTE;

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

		err = vkDeviceWaitIdle(device);
		assert(err == VK_SUCCESS);

	} catch (const exception &e) {
		if (win != nullptr)
			glfwDestroyWindow(win);

#ifdef WIN32
		MessageBox(nullptr, e.what(), nullptr, MB_OK);
#else
		fprintf(stderr, "FATAL ERROR: %s\n", e.what());
#endif
	}

	glfwTerminate();
	return 0;
}
