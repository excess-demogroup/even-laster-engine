#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
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
#include "core/blobbuilder.h"
#include "swapchain.h"
#include "shader.h"
#include "scene/import-texture.h"
#include "scene/sceneimporter.h"
#include "scenerenderer.h"

#include "sync/sync.h"

const auto beatsPerMinute = 174.0f;
const auto rowsPerBeat = 8;
const auto rowRate = (beatsPerMinute / 60.0) * rowsPerBeat;

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <bass.h>

using namespace vulkan;

using std::vector;
using std::map;
using std::exception;
using std::runtime_error;
using std::string;
using std::max;
using std::min;

static vector<const char *> getRequiredInstanceExtensions()
{
	uint32_t requiredExtentionCount;
	auto tmp = glfwGetRequiredInstanceExtensions(&requiredExtentionCount);
	return vector<const char *>(tmp, tmp + requiredExtentionCount);
}

#include "scene/scene.h"
#include "scene/rendertarget.h"

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

VkPhysicalDevice choosePhysicalDevice()
{
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

	return physicalDevice;
}

enum BlendMode {
	None,
	Additive
};

static VkPipeline createGeometrylessPipeline(VkPipelineLayout layout, VkRenderPass renderPass, const vector<VkPipelineShaderStageCreateInfo> &shaderStages, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, bool depthWrite = true, BlendMode blendMode = None)
{
	VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
	pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
	pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineInputAssemblyStateCreateInfo.topology = topology;
	pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState[1] = { { 0 } };
	pipelineColorBlendAttachmentState[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	switch (blendMode) {
	case BlendMode::None:
		pipelineColorBlendAttachmentState[0].blendEnable = VK_FALSE;
		break;
	case BlendMode::Additive:
		pipelineColorBlendAttachmentState[0].blendEnable = VK_TRUE;
		pipelineColorBlendAttachmentState[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		pipelineColorBlendAttachmentState[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		pipelineColorBlendAttachmentState[0].colorBlendOp = VK_BLEND_OP_ADD;
		pipelineColorBlendAttachmentState[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		pipelineColorBlendAttachmentState[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		pipelineColorBlendAttachmentState[0].alphaBlendOp = VK_BLEND_OP_ADD;
		break;
	}

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
	pipelineDepthStencilStateCreateInfo.depthTestEnable = depthWrite;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWrite;
	pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

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
	pipelineCreateInfo.stageCount = shaderStages.size();
	pipelineCreateInfo.pStages = shaderStages.data();

	VkPipeline pipeline;
	auto err = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	assert(err == VK_SUCCESS);

	return pipeline;
}

static VkPipeline createFullScreenQuadPipeline(VkPipelineLayout layout, VkRenderPass renderPass, VkShaderModule fragmentShader)
{
	vector<VkPipelineShaderStageCreateInfo> shaderStages = { {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_VERTEX_BIT,
		loadShaderModule("data/shaders/fullscreenquad.vert.spv"),
		"main",
		nullptr
	}, {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		fragmentShader,
		"main",
		nullptr
	} };

	return createGeometrylessPipeline(layout, renderPass, shaderStages);
}

#include "FastNoise.h"

static Texture3D generateFractalNoise(int width, int height, int depth)
{
	Texture3D texture(VK_FORMAT_R32G32B32A32_SFLOAT, width, height, depth, 1);

	auto size = sizeof(float) * 4 * width * height * depth;
	auto stagingBuffer = new StagingBuffer(size);
	void *ptr = stagingBuffer->map(0, size);

#if 0
	FastNoise noiseX(1337), noiseY(1338), noiseZ(1339);

	auto type = FastNoise::NoiseType::PerlinFractal;
	noiseX.SetNoiseType(type);
	noiseY.SetNoiseType(type);
	noiseZ.SetNoiseType(type);

	auto octaves = 10;
	noiseX.SetFractalOctaves(octaves);
	noiseY.SetFractalOctaves(octaves);
	noiseZ.SetFractalOctaves(octaves);

	noiseX.SetFrequency(4.0f / width);
	noiseY.SetFrequency(4.0f / height);
	noiseZ.SetFrequency(4.0f / depth);

	FastNoise noises[] = { noiseX, noiseY, noiseZ };

	for (auto z = 0; z < depth; ++z) {
		float zw = float(z) / depth;
		for (auto y = 0; y < height; ++y) {
			float yw = float(y) / height;
			auto row = static_cast<float *>(ptr) + 4 * (z * width * height + y * width);
			for (auto x = 0; x < width; ++x) {
				float xw = float(x) / width;

				for (int i = 0; i < 3; ++i) {
					auto a = noises[i].GetNoise(x,         y,          z);
					auto b = noises[i].GetNoise(x + width, y,          z);
					auto c = noises[i].GetNoise(x,         y + height, z);
					auto d = noises[i].GetNoise(x + width, y + height, z);

					auto e = noises[i].GetNoise(x,         y,          z + depth);
					auto f = noises[i].GetNoise(x + width, y,          z + depth);
					auto g = noises[i].GetNoise(x,         y + height, z + depth);
					auto h = noises[i].GetNoise(x + width, y + height, z + depth);

					float h1 = a * xw + b * (1 - xw);
					float h2 = c * xw + d * (1 - xw);
					float h3 = e * xw + f * (1 - xw);
					float h4 = g * xw + h * (1 - xw);

					float v1 = h1 * yw + h2 * (1 - yw);
					float v2 = h3 * yw + h4 * (1 - yw);

					row[x * 4 + i] = v1 * zw + v2 * (1 - zw);
				}
				row[x * 4 + 3] = 0.0f;
			}
		}
	}
	FILE *fp = fopen("data/fbm.raw", "wb");
	fwrite(ptr, 1, size, fp);
	fclose(fp);
#else
	FILE *fp = fopen("data/fbm.raw", "rb");
	if (!fp)
		throw runtime_error("failed to open FBM cache");
	fread(ptr, 1, size, fp);
	fclose(fp);
#endif
	stagingBuffer->unmap();
	texture.uploadFromStagingBuffer(stagingBuffer, 0);
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

	auto appName = "some excess demo";
#ifndef _DEBUG
	auto width = 1920, height = 1080;
#else
	auto width = 1280, height = 720;
#endif
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

		if (!BASS_Init(-1, 44100, 0, 0, 0))
			throw runtime_error("failed to init bass");

		auto stream = BASS_StreamCreateFile(false, "data/soundtrack.mp3", 0, 0, BASS_MP3_SETPOS | BASS_STREAM_PRESCAN);
		if (!stream)
			throw runtime_error("failed to open tune");

		glfwSetKeyCallback(win, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			});


		auto enabledExtensions = getRequiredInstanceExtensions();
#ifndef NDEBUG
		enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

		instanceInit(appName, enabledExtensions);

		auto physicalDevice = choosePhysicalDevice();
		deviceInit(physicalDevice, [](VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueIndex) {
			return glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, queueIndex) == GLFW_TRUE;
		});

		VkSurfaceKHR surface;
		auto err = glfwCreateWindowSurface(instance, win, nullptr, &surface);
		if (err)
			throw runtime_error("glfwCreateWindowSurface failed!");

		auto swapChain = SwapChain(surface, width, height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		vector<VkFormat> depthCandidates = {
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D16_UNORM,
		};

		auto depthFormat = findBestFormat(depthCandidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		DepthRenderTarget sceneDepthRenderTarget(depthFormat, width, height);
		ColorRenderTarget sceneColorRenderTarget(VK_FORMAT_R16G16B16A16_SFLOAT, width, height, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

		int bloomLevels = 32 - clz(max(width, height));
		ColorRenderTarget bloomRenderTarget(VK_FORMAT_R16G16B16A16_SFLOAT, width, height, bloomLevels, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		ColorRenderTarget bloomUpscaleRenderTarget(VK_FORMAT_R16G16B16A16_SFLOAT, width, height, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		Texture2DArrayRenderTarget colorArray(VK_FORMAT_A2B10G10R10_UNORM_PACK32, width, height, 128, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		ColorRenderTarget postProcessRenderTarget(VK_FORMAT_A2B10G10R10_UNORM_PACK32, width, height, 1, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

		vector<VkAttachmentDescription> sceneRenderPassAttachments;
		VkAttachmentDescription sceneDepthAttachment;
		sceneDepthAttachment.flags = 0;
		sceneDepthAttachment.format = depthFormat;
		sceneDepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		sceneDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		sceneDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		sceneDepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		sceneDepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		sceneDepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		sceneDepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		sceneRenderPassAttachments.push_back(sceneDepthAttachment);

		VkAttachmentDescription sceneColorAttachment;
		sceneColorAttachment.flags = 0;
		sceneColorAttachment.format = sceneColorRenderTarget.getFormat();
		sceneColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		sceneColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		sceneColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		sceneColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		sceneColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		sceneColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		sceneColorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		sceneRenderPassAttachments.push_back(sceneColorAttachment);

		VkAttachmentReference sceneDepthAttachmentReference = {};
		sceneDepthAttachmentReference.attachment = 0;
		sceneDepthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference sceneColorAttachmentReference = {};
		sceneColorAttachmentReference.attachment = 1;
		sceneColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription sceneSubpass = {};
		sceneSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		sceneSubpass.colorAttachmentCount = 1;
		sceneSubpass.pColorAttachments = &sceneColorAttachmentReference;
		sceneSubpass.pDepthStencilAttachment = &sceneDepthAttachmentReference;

		VkRenderPassCreateInfo sceneRenderPassCreateInfo = {};
		sceneRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		sceneRenderPassCreateInfo.attachmentCount = sceneRenderPassAttachments.size();
		sceneRenderPassCreateInfo.pAttachments = sceneRenderPassAttachments.data();
		sceneRenderPassCreateInfo.subpassCount = 1;
		sceneRenderPassCreateInfo.pSubpasses = &sceneSubpass;

		VkRenderPass sceneRenderPass;
		err = vkCreateRenderPass(device, &sceneRenderPassCreateInfo, nullptr, &sceneRenderPass);
		assert(err == VK_SUCCESS);

		auto sceneFramebuffer = createFramebuffer(
			width, height, 1,
			{ sceneDepthRenderTarget.getImageView(), sceneColorRenderTarget.getImageView() },
			sceneRenderPass);

		VkAttachmentDescription bloomColorAttachment;
		bloomColorAttachment.flags = 0;
		bloomColorAttachment.format = bloomRenderTarget.getFormat();
		bloomColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		bloomColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		bloomColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		bloomColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		bloomColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		bloomColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		bloomColorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference bloomColorAttachmentReference = {};
		bloomColorAttachmentReference.attachment = 0;
		bloomColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription bloomSubpass = {};
		bloomSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		bloomSubpass.colorAttachmentCount = 1;
		bloomSubpass.pColorAttachments = &bloomColorAttachmentReference;

		VkRenderPassCreateInfo bloomRenderPassCreateInfo = {};
		bloomRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		bloomRenderPassCreateInfo.attachmentCount = 1;
		bloomRenderPassCreateInfo.pAttachments = &bloomColorAttachment;
		bloomRenderPassCreateInfo.subpassCount = 1;
		bloomRenderPassCreateInfo.pSubpasses = &bloomSubpass;

		VkRenderPass bloomRenderPass;
		err = vkCreateRenderPass(device, &bloomRenderPassCreateInfo, nullptr, &bloomRenderPass);
		assert(err == VK_SUCCESS);

		auto bloomDescriptorSetLayout = createDescriptorSetLayout({
			{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		});

		auto bloomUpscaleDescriptorSetLayout = createDescriptorSetLayout({
			{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
		});

		auto bloomDescriptorPool = createDescriptorPool({
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uint32_t(bloomLevels + 2) },
		}, bloomLevels + 1);

		vector<VkFramebuffer> bloomFramebuffers;
		vector<VkDescriptorSet> bloomDescriptorSets;
		vector<VkImageView> bloomImageViews;

		VkSampler bloomInputSampler = createSampler(0.0f, false, false);
		for (int mipLevel = 0; mipLevel < bloomLevels; ++mipLevel) {
			VkImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = mipLevel;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 1;
			auto imageView = createImageView(bloomRenderTarget.getImage(), VK_IMAGE_VIEW_TYPE_2D, bloomRenderTarget.getFormat(), subresourceRange);
			bloomImageViews.push_back(imageView);

			auto mipWidth = TextureBase::mipSize(bloomRenderTarget.getWidth(), mipLevel);
			auto mipHeight = TextureBase::mipSize(bloomRenderTarget.getHeight(), mipLevel);
			auto framebuffer = createFramebuffer(mipWidth, mipHeight, 1, { imageView }, bloomRenderPass);
			bloomFramebuffers.push_back(framebuffer);

			auto descriptorSet = allocateDescriptorSet(bloomDescriptorPool, bloomDescriptorSetLayout);

			VkDescriptorImageInfo descriptorImageInfo = {};
			descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			if (mipLevel == 0)
				descriptorImageInfo.imageView = sceneColorRenderTarget.getImageView();
			else
				descriptorImageInfo.imageView = bloomImageViews[mipLevel - 1];
			descriptorImageInfo.sampler = bloomInputSampler;

			VkWriteDescriptorSet writeDescriptorSet = {};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = descriptorSet;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSet.pBufferInfo = nullptr;
			writeDescriptorSet.pImageInfo = &descriptorImageInfo;
			writeDescriptorSet.dstBinding = 0;
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

			bloomDescriptorSets.push_back(descriptorSet);
		}

		auto bloomPipelineLayout = createPipelineLayout({ bloomDescriptorSetLayout }, {});
		auto bloomFragmentShader = loadShaderModule("data/shaders/bloom.frag.spv");
		auto bloomPipeline = createFullScreenQuadPipeline(bloomPipelineLayout, bloomRenderPass, bloomFragmentShader);

		VkAttachmentDescription bloomUpscaleColorAttachment;
		bloomUpscaleColorAttachment.flags = 0;
		bloomUpscaleColorAttachment.format = bloomUpscaleRenderTarget.getFormat();
		bloomUpscaleColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		bloomUpscaleColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		bloomUpscaleColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		bloomUpscaleColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		bloomUpscaleColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		bloomUpscaleColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		bloomUpscaleColorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		VkAttachmentReference bloomUpscaleColorAttachmentReference = {};
		bloomUpscaleColorAttachmentReference.attachment = 0;
		bloomUpscaleColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription bloomUpscaleSubpass = {};
		bloomUpscaleSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		bloomUpscaleSubpass.colorAttachmentCount = 1;
		bloomUpscaleSubpass.pColorAttachments = &bloomUpscaleColorAttachmentReference;

		VkRenderPassCreateInfo bloomUpscaleRenderPassCreateInfo = {};
		bloomUpscaleRenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		bloomUpscaleRenderPassCreateInfo.attachmentCount = 1;
		bloomUpscaleRenderPassCreateInfo.pAttachments = &bloomUpscaleColorAttachment;
		bloomUpscaleRenderPassCreateInfo.subpassCount = 1;
		bloomUpscaleRenderPassCreateInfo.pSubpasses = &bloomUpscaleSubpass;

		VkRenderPass bloomUpscaleRenderPass;
		err = vkCreateRenderPass(device, &bloomUpscaleRenderPassCreateInfo, nullptr, &bloomUpscaleRenderPass);
		assert(err == VK_SUCCESS);

		auto bloomUpscaleFramebuffer = createFramebuffer(width, height, 1, { bloomUpscaleRenderTarget.getImageView() }, bloomUpscaleRenderPass);
		auto bloomUpscaleDescriptorSet = allocateDescriptorSet(bloomDescriptorPool, bloomUpscaleDescriptorSetLayout);
		VkSampler bloomSampler = createSampler(float(bloomLevels), false, false);

		{
			VkDescriptorImageInfo postProcessRenderTargetImageInfo = {};
			postProcessRenderTargetImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			postProcessRenderTargetImageInfo.imageView = postProcessRenderTarget.getImageView();

			VkWriteDescriptorSet writeDescriptorSet = {};

			vector<VkDescriptorImageInfo> descriptorImageInfos = {
				{ bloomSampler, sceneColorRenderTarget.getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
				{ bloomSampler, bloomRenderTarget.getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			};

			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = bloomUpscaleDescriptorSet;
			writeDescriptorSet.dstBinding = 0;
			writeDescriptorSet.descriptorCount = descriptorImageInfos.size();
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSet.pImageInfo = descriptorImageInfos.data();

			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}

		struct {
			float bloomAmount;
			float bloomShape;
			float seed;
		} bloomUpscalePushConstants;

		VkPushConstantRange bloomUpscalePushConstantRange = {
			VK_SHADER_STAGE_FRAGMENT_BIT,
			0,
			sizeof(bloomUpscalePushConstants)
		};
		auto bloomUpscalePipelineLayout = createPipelineLayout({ bloomUpscaleDescriptorSetLayout }, { bloomUpscalePushConstantRange });
		auto bloomUpscaleFragmentShader = loadShaderModule("data/shaders/bloom_upscale.frag.spv");
		auto bloomUpscalePipeline = createFullScreenQuadPipeline(bloomUpscalePipelineLayout, bloomUpscaleRenderPass, bloomUpscaleFragmentShader);

		struct {
			glm::mat4 modelViewMatrix;
			glm::mat4 modelViewInverseMatrix;
			glm::mat4 modelViewProjectionMatrix;
			glm::vec2 offset;
			glm::vec2 scale;
			float time;
		} wavePlaneUniforms;
		auto wavePlaneUniformBuffer = new Buffer(sizeof(wavePlaneUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		auto wavePlaneDescriptorSetLayout = createDescriptorSetLayout({
			{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT },
		});
		auto wavePlanePipelineLayout = createPipelineLayout({ wavePlaneDescriptorSetLayout }, {});
		vector<VkPipelineShaderStageCreateInfo> wavePlaneShaderStages = { {
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				nullptr,
				0,
				VK_SHADER_STAGE_VERTEX_BIT,
				loadShaderModule("data/shaders/plane.vert.spv"),
				"main",
				nullptr
			},{
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				nullptr,
				0,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				loadShaderModule("data/shaders/plane.frag.spv"),
				"main",
				nullptr
			} };

		auto wavePlanePipeline = createGeometrylessPipeline(wavePlanePipelineLayout, sceneRenderPass, wavePlaneShaderStages, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, false, BlendMode::Additive);

		auto wavePlaneDescriptorPool = createDescriptorPool({
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
			}, 1);

		auto wavePlaneDescriptorSet = allocateDescriptorSet(wavePlaneDescriptorPool, wavePlaneDescriptorSetLayout);

		Texture3D fractalNoise = generateFractalNoise(64, 64, 64);
		VkSampler fractalNoiseSampler = createSampler(0.0f, true, false);

		{
			VkDescriptorImageInfo postProcessRenderTargetImageInfo = {};
			postProcessRenderTargetImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			postProcessRenderTargetImageInfo.imageView = postProcessRenderTarget.getImageView();

			VkWriteDescriptorSet writeDescriptorSets[2] = {};

			auto descriptorBufferInfo = wavePlaneUniformBuffer->getDescriptorBufferInfo();
			writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[0].dstSet = wavePlaneDescriptorSet;
			writeDescriptorSets[0].dstBinding = 0;
			writeDescriptorSets[0].descriptorCount = 1;
			writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSets[0].pBufferInfo = &descriptorBufferInfo;

			vector<VkDescriptorImageInfo> descriptorImageInfos = {
				{ fractalNoiseSampler, fractalNoise.getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			};

			writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[1].dstSet = wavePlaneDescriptorSet;
			writeDescriptorSets[1].dstBinding = 1;
			writeDescriptorSets[1].descriptorCount = descriptorImageInfos.size();
			writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[1].pImageInfo = descriptorImageInfos.data();

			vkUpdateDescriptorSets(device, ARRAY_SIZE(writeDescriptorSets), writeDescriptorSets, 0, nullptr);
		}

		vector<Scene *> scenes;
		for (int i = 0; true; ++i) {
			char path[256];
			snprintf(path, sizeof(path), "assets/scenes/%04d.dae", i);

			struct stat st;
			if (stat(path, &st) < 0 ||
				(st.st_mode & _S_IFMT) != S_IFREG)
				break;

			VkFormat format = VK_FORMAT_UNDEFINED;
			scenes.push_back(SceneImporter::import(path));
		}

		vector<SceneRenderer> sceneRenderers;
		for (auto scene : scenes)
			sceneRenderers.push_back(SceneRenderer(scene, sceneRenderPass));

		auto planes = importTexture2DArray("assets/planes", TextureImportFlags::NONE);
		auto offsetMaps = importTexture2DArray("assets/offset-maps", TextureImportFlags::NONE);
		auto overlays = importTexture2DArray("assets/overlays", TextureImportFlags::PREMULTIPLY_ALPHA);

		VkSampler textureSampler = createSampler(float(planes.getMipLevels()), false, false);
		VkDescriptorImageInfo descriptorImageInfo = planes.getDescriptorImageInfo(textureSampler);

		struct {
			float planeIndex;
			float fade;
			float refractiveIndex;
		} refractionUniforms;
		auto refractionUniformBuffer = new Buffer(sizeof(refractionUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		for (SceneRenderer &sceneRenderer : sceneRenderers) {
			VkWriteDescriptorSet writeDescriptorSets[2] = {};
			writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[0].dstSet = sceneRenderer.getDescriptorSet();
			writeDescriptorSets[0].descriptorCount = 1;
			writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[0].pBufferInfo = nullptr;
			writeDescriptorSets[0].pImageInfo = &descriptorImageInfo;
			writeDescriptorSets[0].dstBinding = 1;

			auto descriptorBufferInfo = refractionUniformBuffer->getDescriptorBufferInfo();
			writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[1].dstSet = sceneRenderer.getDescriptorSet();
			writeDescriptorSets[1].descriptorCount = 1;
			writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeDescriptorSets[1].pBufferInfo = &descriptorBufferInfo;
			writeDescriptorSets[1].dstBinding = 2;

			vkUpdateDescriptorSets(device, ARRAY_SIZE(writeDescriptorSets), writeDescriptorSets, 0, nullptr);
		}

		VkSampler arrayTextureSampler = createSampler(0.0f, false, false);

		auto postProcessDescriptorSetLayout = createDescriptorSetLayout({
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0 },
			{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
			{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
			{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
			});

		struct {
			uint32_t arrayBufferFrame;
			uint32_t validFrames;
			uint32_t delayImage;
			uint32_t overlayIndex;
			float delayAmount;
			float delayChroma;
			float overlayAlpha;
			float fade;
			float flash;
		} postProcessPushConstantData;

		VkPushConstantRange postProcessPushConstantRange = {
			VK_SHADER_STAGE_COMPUTE_BIT,
			0,
			sizeof(postProcessPushConstantData)
		};

		auto postProcessPipelineLayout = createPipelineLayout({ postProcessDescriptorSetLayout }, { postProcessPushConstantRange });

		VkPipeline postProcessPipeline = createComputePipeline(postProcessPipelineLayout, loadShaderModule("data/shaders/postprocess.comp.spv"));

		auto postProcessDescriptorPool = createDescriptorPool({
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
		}, swapChain.getImageViews().size());

		auto postProcessDescriptorSet = allocateDescriptorSet(postProcessDescriptorPool, postProcessDescriptorSetLayout);
		{
			VkDescriptorImageInfo postProcessRenderTargetImageInfo = {};
			postProcessRenderTargetImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			postProcessRenderTargetImageInfo.imageView = postProcessRenderTarget.getImageView();

			VkWriteDescriptorSet writeDescriptorSets[2] = {};
			writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[0].dstSet = postProcessDescriptorSet;
			writeDescriptorSets[0].dstBinding = 0;
			writeDescriptorSets[0].descriptorCount = 1;
			writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDescriptorSets[0].pImageInfo = &postProcessRenderTargetImageInfo;

			vector<VkDescriptorImageInfo> descriptorImageInfos = {
				{ arrayTextureSampler, colorArray.getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
				{ arrayTextureSampler, offsetMaps.getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
				{ arrayTextureSampler, overlays.getImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			};

			VkDescriptorImageInfo descriptorImageInfo1 = {};
			descriptorImageInfo1.sampler = arrayTextureSampler;
			descriptorImageInfo1.imageView = colorArray.getImageView();
			descriptorImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[1].dstSet = postProcessDescriptorSet;
			writeDescriptorSets[1].dstBinding = 1;
			writeDescriptorSets[1].descriptorCount = descriptorImageInfos.size();
			writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[1].pImageInfo = descriptorImageInfos.data();

			vkUpdateDescriptorSets(device, ARRAY_SIZE(writeDescriptorSets), writeDescriptorSets, 0, nullptr);
		}


		auto backBufferSemaphore = createSemaphore(),
		     presentCompleteSemaphore = createSemaphore();

		VkCommandPool commandPool = createCommandPool(graphicsQueueIndex);

		auto commandBuffers = allocateCommandBuffers(commandPool, swapChain.getImageViews().size());

		auto commandBufferFences = new VkFence[commandBuffers.size()];
		for (auto i = 0u; i < commandBuffers.size(); ++i)
			commandBufferFences[i] = createFence(VK_FENCE_CREATE_SIGNALED_BIT);

		err = vkQueueWaitIdle(graphicsQueue);
		assert(err == VK_SUCCESS);

		auto rocket = sync_create_device("data/sync");
		if (!rocket)
			throw runtime_error("sync_create_device() failed: out of memory?");

#ifndef SYNC_PLAYER
		if (sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT))
			throw runtime_error("failed to connect to host");
#endif

		auto sceneIndexTrack = sync_get_track(rocket, "scene.index");

		auto clearRTrack = sync_get_track(rocket, "background:clear.r");
		auto clearGTrack = sync_get_track(rocket, "background:clear.g");
		auto clearBTrack = sync_get_track(rocket, "background:clear.b");

		auto cameraFOVTrack = sync_get_track(rocket, "camera:fov");
		auto cameraRotYTrack = sync_get_track(rocket, "camera:rot.y");
		auto cameraDistTrack = sync_get_track(rocket, "camera:dist");
		auto cameraRollTrack = sync_get_track(rocket, "camera:roll");
		auto cameraUpTrack = sync_get_track(rocket, "camera:up");
		auto cameraTargetXTrack = sync_get_track(rocket, "camera:target.x");
		auto cameraTargetYTrack = sync_get_track(rocket, "camera:target.y");
		auto cameraTargetZTrack = sync_get_track(rocket, "camera:target.z");

		auto refractionPlaneIndexTrack = sync_get_track(rocket, "refraction:plane");
		auto refractionFadeTrack = sync_get_track(rocket, "refraction:fade");
		auto refractionIndexTrack = sync_get_track(rocket, "refraction:index");

		auto delayImageTrack = sync_get_track(rocket, "postprocess:delay.image");
		auto delayAmountTrack = sync_get_track(rocket, "postprocess:delay.amount");
		auto delayChromaTrack = sync_get_track(rocket, "postprocess:delay.chroma");
		auto bloomAmountTrack = sync_get_track(rocket, "postprocess:bloom.amount");
		auto bloomShapeTrack = sync_get_track(rocket, "postprocess:bloom.shape");

		auto overlayIndexTrack = sync_get_track(rocket, "overlay.index");
		auto overlayAlphaTrack = sync_get_track(rocket, "overlay.alpha");
		auto fadeTrack = sync_get_track(rocket, "fade");
		auto flashTrack = sync_get_track(rocket, "flash");
		auto pulseAmountTrack = sync_get_track(rocket, "pulse.amount");
		auto pulseSpeedTrack = sync_get_track(rocket, "pulse.speed");

		auto wavePlaneOffsetXTrack = sync_get_track(rocket, "waveplane:offset.x");
		auto wavePlaneOffsetYTrack = sync_get_track(rocket, "waveplane:offset.y");
		auto wavePlaneScaleXTrack = sync_get_track(rocket, "waveplane:scale.x");
		auto wavePlaneScaleYTrack = sync_get_track(rocket, "waveplane:scale.y");
		auto wavePlaneTimeTrack = sync_get_track(rocket, "waveplane:time");

		BASS_Start();
		BASS_ChannelPlay(stream, false);

		int validFrames = 0;
		while (!glfwWindowShouldClose(win)) {
			auto pos = BASS_ChannelGetPosition(stream, BASS_POS_BYTE);
			auto time = BASS_ChannelBytes2Seconds(stream, pos);
			auto row = time * rowRate;

#ifndef SYNC_PLAYER
			static sync_cb bassCallbacks = {
				// pause
				[](void *d, int flag) {
					HSTREAM h = *((HSTREAM *)d);
					if (flag)
						BASS_ChannelPause(h);
					else
						BASS_ChannelPlay(h, false);
				},
					// set row
					[](void *d, int row) {
					HSTREAM h = *((HSTREAM *)d);
					QWORD pos = BASS_ChannelSeconds2Bytes(h, (row + 0.01) / rowRate);
					BASS_ChannelSetPosition(h, pos, BASS_POS_BYTE);
				},

					// is playing
					[](void *d) -> int {
					HSTREAM h = *((HSTREAM *)d);
					return BASS_ChannelIsActive(h) == BASS_ACTIVE_PLAYING;
				},
			};

			if (sync_update(rocket, int(floor(row)), &bassCallbacks, (void *)&stream))
				sync_tcp_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
#endif

			auto currentSwapImage = swapChain.aquireNextImage(backBufferSemaphore);
			static int nextArrayBufferFrame = 0;
			int arrayBufferFrame = nextArrayBufferFrame++;
			uint32_t arrayBufferFrameWrapped = arrayBufferFrame % colorArray.getArrayLayers();

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
				float(sync_get_val(clearRTrack, row)),
				float(sync_get_val(clearGTrack, row)),
				float(sync_get_val(clearBTrack, row)),
				1.0f
			};

			VkRenderPassBeginInfo sceneRenderPassBegin = {};
			sceneRenderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			sceneRenderPassBegin.renderPass = sceneRenderPass;
			sceneRenderPassBegin.renderArea.offset.x = 0;
			sceneRenderPassBegin.renderArea.offset.y = 0;
			sceneRenderPassBegin.renderArea.extent.width = width;
			sceneRenderPassBegin.renderArea.extent.height = height;
			sceneRenderPassBegin.clearValueCount = ARRAY_SIZE(clearValues);
			sceneRenderPassBegin.pClearValues = clearValues;
			sceneRenderPassBegin.framebuffer = sceneFramebuffer;

			vkCmdBeginRenderPass(commandBuffer, &sceneRenderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

			setViewport(commandBuffer, 0, 0, float(width), float(height));
			setScissor(commandBuffer, 0, 0, width, height);

			auto th = sync_get_val(cameraRotYTrack, row) * (M_PI / 180);
			auto dist = sync_get_val(cameraDistTrack, row);
			auto roll = sync_get_val(cameraRollTrack, row) * (M_PI / 180);

			auto cameraTargetX = sync_get_val(cameraTargetXTrack, row);
			auto cameraTargetY = sync_get_val(cameraTargetYTrack, row);
			auto cameraTargetZ = sync_get_val(cameraTargetZTrack, row);

			auto targetPosition = glm::vec3(
				float(cameraTargetX),
				float(cameraTargetY),
				float(cameraTargetZ));
			auto viewPosition = glm::vec3(
				cameraTargetX + sin(th) * dist,
				cameraTargetY + sync_get_val(cameraUpTrack, row),
				cameraTargetZ + cos(th) * dist);
			auto lookAt = glm::lookAt(viewPosition, targetPosition, glm::vec3(0, 1, 0));
			auto viewMatrix = glm::rotate(glm::mat4(1), float(roll), glm::vec3(0, 0, 1)) * lookAt;

			auto fov = sync_get_val(cameraFOVTrack, row);
			auto aspect = float(width) / height;
			auto znear = 0.01f;
			auto zfar = 100.0f;
			auto projectionMatrix = glm::perspective(float(fov * M_PI / 180), aspect, znear, zfar);

			int sceneIndex = int(sync_get_val(sceneIndexTrack, row));
			if (sceneIndex >= 0) {
				sceneIndex %= sceneRenderers.size();
				SceneRenderer &sceneRenderer = sceneRenderers[sceneIndex];

				refractionUniforms.planeIndex = float(sync_get_val(refractionPlaneIndexTrack, row));
				refractionUniforms.fade = float(sync_get_val(refractionFadeTrack, row));
				refractionUniforms.refractiveIndex = float(sync_get_val(refractionIndexTrack, row));

				auto ptr = refractionUniformBuffer->map(0, sizeof(refractionUniforms));
				memcpy(ptr, &refractionUniforms, sizeof(refractionUniforms));
				refractionUniformBuffer->unmap();

				sceneRenderer.draw(commandBuffer, viewMatrix, projectionMatrix);
			} else {
				int size = 256;

				auto modelMatrix = glm::mat4(1);
				auto modelViewMatrix = viewMatrix * modelMatrix;
				auto modelViewProjectionMatrix = projectionMatrix * modelViewMatrix;
				wavePlaneUniforms.modelViewMatrix = modelViewMatrix;
				wavePlaneUniforms.modelViewInverseMatrix = glm::inverse(modelViewMatrix);
				wavePlaneUniforms.modelViewProjectionMatrix = modelViewProjectionMatrix;

				wavePlaneUniforms.offset = glm::vec2(sync_get_val(wavePlaneOffsetXTrack, row),
				                                     sync_get_val(wavePlaneOffsetYTrack, row));
				wavePlaneUniforms.scale = glm::vec2(sync_get_val(wavePlaneScaleXTrack, row),
				                                    sync_get_val(wavePlaneScaleYTrack, row));
				wavePlaneUniforms.time = float(sync_get_val(wavePlaneTimeTrack, row));

				auto ptr = wavePlaneUniformBuffer->map(0, sizeof(wavePlaneUniforms));
				memcpy(ptr, &wavePlaneUniforms, sizeof(wavePlaneUniforms));
				wavePlaneUniformBuffer->unmap();

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,  wavePlanePipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wavePlanePipelineLayout, 0, 1, &wavePlaneDescriptorSet, 0, nullptr);
	
				for (int i = 0; i < size; ++i)
					vkCmdDraw(commandBuffer, 2 + 2 * size, 1, (1 << 16) * i, 0);
			}

			vkCmdEndRenderPass(commandBuffer);

			for (int i = 0; i < bloomLevels; ++i) {
				int levelWidth = TextureBase::mipSize(bloomRenderTarget.getWidth(), i);
				int levelHeight = TextureBase::mipSize(bloomRenderTarget.getHeight(), i);
				VkRenderPassBeginInfo bloomRenderPassBegin = {};
				bloomRenderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				bloomRenderPassBegin.renderPass = bloomRenderPass;
				bloomRenderPassBegin.renderArea.offset.x = 0;
				bloomRenderPassBegin.renderArea.offset.y = 0;
				bloomRenderPassBegin.renderArea.extent.width = levelWidth;
				bloomRenderPassBegin.renderArea.extent.height = levelHeight;
				bloomRenderPassBegin.framebuffer = bloomFramebuffers[i];

				vkCmdBeginRenderPass(commandBuffer, &bloomRenderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

				setViewport(commandBuffer, 0, 0, float(levelWidth), float(levelHeight));
				setScissor(commandBuffer, 0, 0, levelWidth, levelHeight);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomPipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomPipelineLayout, 0, 1, &bloomDescriptorSets[i], 0, nullptr);
				vkCmdDraw(commandBuffer, 3, 1, 0, 0);

				vkCmdEndRenderPass(commandBuffer);
			}

			VkRenderPassBeginInfo bloomUpscaleRenderPassBegin = {};
			bloomUpscaleRenderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			bloomUpscaleRenderPassBegin.renderPass = bloomUpscaleRenderPass;
			bloomUpscaleRenderPassBegin.renderArea.offset.x = 0;
			bloomUpscaleRenderPassBegin.renderArea.offset.y = 0;
			bloomUpscaleRenderPassBegin.renderArea.extent.width = width;
			bloomUpscaleRenderPassBegin.renderArea.extent.height = height;
			bloomUpscaleRenderPassBegin.framebuffer = bloomUpscaleFramebuffer;

			vkCmdBeginRenderPass(commandBuffer, &bloomUpscaleRenderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

			setViewport(commandBuffer, 0, 0, float(width), float(height));
			setScissor(commandBuffer, 0, 0, width, height);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomUpscalePipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomUpscalePipelineLayout, 0, 1, &bloomUpscaleDescriptorSet, 0, nullptr);

			bloomUpscalePushConstants.bloomAmount = float(sync_get_val(bloomAmountTrack, row));
			bloomUpscalePushConstants.bloomShape = float(sync_get_val(bloomShapeTrack, row));
			bloomUpscalePushConstants.seed = float(rand()) / RAND_MAX;
			vkCmdPushConstants(commandBuffer, bloomUpscalePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(bloomUpscalePushConstants), &bloomUpscalePushConstants);

			vkCmdDraw(commandBuffer, 3, 1, 0, 0);

			vkCmdEndRenderPass(commandBuffer);

			imageBarrier(
				commandBuffer,
				colorArray.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			blitImage(commandBuffer,
				bloomUpscaleRenderTarget.getImage(),
				colorArray.getImage(),
				width, height,
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, arrayBufferFrameWrapped, 1 });

			imageBarrier(
				commandBuffer,
				colorArray.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_ACCESS_TRANSFER_WRITE_BIT, 0,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			if (validFrames < colorArray.getArrayLayers())
				validFrames++;

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, postProcessPipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, postProcessPipelineLayout, 0, 1, &postProcessDescriptorSet, 0, nullptr);

			imageBarrier(
				commandBuffer,
				postProcessRenderTarget.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			auto fade = sync_get_val(fadeTrack, row);
			auto pulseAmount = sync_get_val(pulseAmountTrack, row);
			auto pulseSpeed = sync_get_val(pulseSpeedTrack, row);
			fade = max(0.0, fade - pulseAmount + float(cos(row * pulseSpeed * (M_PI / rowsPerBeat))) * pulseAmount);

			postProcessPushConstantData.arrayBufferFrame = uint32_t(arrayBufferFrame);
			postProcessPushConstantData.validFrames = uint32_t(validFrames);
			postProcessPushConstantData.delayImage = uint32_t(sync_get_val(delayImageTrack, row));
			postProcessPushConstantData.overlayIndex = uint32_t(sync_get_val(overlayIndexTrack, row));
			postProcessPushConstantData.delayAmount = float(sync_get_val(delayAmountTrack, row));
			postProcessPushConstantData.delayChroma = float(1.0 - min(max(0.0, sync_get_val(delayChromaTrack, row)), 1.0));
			postProcessPushConstantData.overlayAlpha = float(sync_get_val(overlayAlphaTrack, row));
			postProcessPushConstantData.fade = float(fade);
			postProcessPushConstantData.flash = float(sync_get_val(flashTrack, row));
			vkCmdPushConstants(commandBuffer, postProcessPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(postProcessPushConstantData), &postProcessPushConstantData);

			vkCmdDispatch(commandBuffer, width / 16, height / 16, 1);

			imageBarrier(
				commandBuffer,
				postProcessRenderTarget.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			auto swapChainImage = swapChain.getImages()[currentSwapImage];
			imageBarrier(
				commandBuffer,
				swapChainImage,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			blitImage(commandBuffer,
				postProcessRenderTarget.getImage(),
				swapChainImage,
				width, height,
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 });

			imageBarrier(
				commandBuffer,
				swapChainImage,
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

#ifndef SYNC_PLAYER
		sync_save_tracks(rocket);
#endif
		sync_destroy_device(rocket);

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
