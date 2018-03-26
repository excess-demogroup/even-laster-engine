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
		loadShaderModule("data/shaders/refraction.vert.spv"),
		"main",
		NULL
	}, {
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		nullptr,
		0,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		loadShaderModule("data/shaders/refraction.frag.spv"),
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
	auto width = 1920, height = 1080;
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
		DepthRenderTarget depthRenderTarget(depthFormat, width, height);

		auto renderTargetFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		ColorRenderTarget colorRenderTarget(renderTargetFormat, width, height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		Texture2DArrayRenderTarget colorArray(renderTargetFormat, width, height, 128, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		ColorRenderTarget computeRenderTarget(renderTargetFormat, width, height, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

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
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

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

		auto framebuffer = createFramebuffer(
			width, height, 1,
			{ depthRenderTarget.getImageView(), colorRenderTarget.getImageView() },
			renderPass);

		auto imageViews = swapChain.getImageViews();
		auto images = swapChain.getImages();

		vector<Scene *> scenes = {
			SceneImporter::import("assets/scenes/0000.dae"),
			SceneImporter::import("assets/scenes/0001.dae")
		};

		vector<SceneRenderer> sceneRenderers;
		for (auto scene : scenes)
			sceneRenderers.push_back(SceneRenderer(scene, renderPass));

		// OK, let's prepare for rendering!

		auto texture = importTexture2D("assets/excess-logo.png", TextureImportFlags::GENERATE_MIPMAPS);
		auto offsetMaps = importTexture2DArray("assets/offset-maps", TextureImportFlags::NONE);

		VkSampler textureSampler = createSampler(float(texture.getMipLevels()), false, false);
		VkDescriptorImageInfo descriptorImageInfo = texture.getDescriptorImageInfo(textureSampler);

		for (SceneRenderer &sceneRenderer : sceneRenderers) {
			VkWriteDescriptorSet writeDescriptorSet = {};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = sceneRenderer.getDescriptorSet();
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSet.pBufferInfo = nullptr;
			writeDescriptorSet.pImageInfo = &descriptorImageInfo;
			writeDescriptorSet.dstBinding = 1;
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}

		VkSampler arrayTextureSampler = createSampler(1.0f, false, false);

		auto computeDescriptorSetLayout = createDescriptorSetLayout({
			{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, 0 },
			{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
			{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT },
		});

		struct {
			uint32_t arrayBufferFrame;
			uint32_t validFrames;
			uint32_t delayImage;
			float delayAmount;
			float delayChroma;
		} pushConstantData;

		VkPushConstantRange pushConstantRange = {
			VK_SHADER_STAGE_COMPUTE_BIT,
			0,
			sizeof(pushConstantData)
		};

		auto computePipelineLayout = createPipelineLayout({ computeDescriptorSetLayout }, { pushConstantRange });

		VkPipeline computePipeline = createComputePipeline(computePipelineLayout, loadShaderModule("data/shaders/postprocess.comp.spv"));

		auto computeDescriptorPool = createDescriptorPool({
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, uint32_t(imageViews.size()) },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, uint32_t(imageViews.size() * 2) },
		}, imageViews.size());

		auto computeDescriptorSet = allocateDescriptorSet(computeDescriptorPool, computeDescriptorSetLayout);
		{
			VkDescriptorImageInfo computeDescriptorImageInfo = {};
			computeDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			computeDescriptorImageInfo.imageView = computeRenderTarget.getImageView();

			VkWriteDescriptorSet writeDescriptorSets[3] = {};
			writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[0].dstSet = computeDescriptorSet;
			writeDescriptorSets[0].dstBinding = 0;
			writeDescriptorSets[0].descriptorCount = 1;
			writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDescriptorSets[0].pImageInfo = &computeDescriptorImageInfo;

			VkDescriptorImageInfo descriptorImageInfo1 = {};
			descriptorImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfo1.imageView = colorArray.getImageView();
			descriptorImageInfo1.sampler = arrayTextureSampler;

			writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[1].dstSet = computeDescriptorSet;
			writeDescriptorSets[1].dstBinding = 1;
			writeDescriptorSets[1].descriptorCount = 1;
			writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[1].pImageInfo = &descriptorImageInfo1;

			VkDescriptorImageInfo descriptorImageInfo2 = {};
			descriptorImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfo2.imageView = offsetMaps.getImageView();
			descriptorImageInfo2.sampler = arrayTextureSampler;

			writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[2].dstSet = computeDescriptorSet;
			writeDescriptorSets[2].dstBinding = 2;
			writeDescriptorSets[2].descriptorCount = 1;
			writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[2].pImageInfo = &descriptorImageInfo2;

			vkUpdateDescriptorSets(device, ARRAY_SIZE(writeDescriptorSets), writeDescriptorSets, 0, nullptr);
		}


		auto backBufferSemaphore = createSemaphore(),
		     presentCompleteSemaphore = createSemaphore();

		VkCommandPool commandPool = createCommandPool(graphicsQueueIndex);
		auto commandBuffers = allocateCommandBuffers(commandPool, imageViews.size());

		auto commandBufferFences = new VkFence[imageViews.size()];
		for (auto i = 0u; i < imageViews.size(); ++i)
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

		auto clear_r = sync_get_track(rocket, "background:clear.r");
		auto clear_g = sync_get_track(rocket, "background:clear.g");
		auto clear_b = sync_get_track(rocket, "background:clear.b");

		auto cam_rot = sync_get_track(rocket, "camera:rot.y");
		auto cam_dist = sync_get_track(rocket, "camera:dist");
		auto cam_roll = sync_get_track(rocket, "camera:roll");

		auto pp_delay_image = sync_get_track(rocket, "postprocess:delay.image");
		auto pp_delay_amount = sync_get_track(rocket, "postprocess:delay.amount");
		auto pp_delay_chroma = sync_get_track(rocket, "postprocess:delay.chroma");

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
					QWORD pos = BASS_ChannelSeconds2Bytes(h, row / rowRate);
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
				float(sync_get_val(clear_r, row)),
				float(sync_get_val(clear_g, row)),
				float(sync_get_val(clear_b, row)),
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

			auto th = sync_get_val(cam_rot, row) * (M_PI / 180);
			auto dist = sync_get_val(cam_dist, row);
			auto roll = sync_get_val(cam_roll, row) * (M_PI / 180);

			auto viewPosition = glm::vec3(sin(th) * dist, 0.0f, cos(th) * dist);
			auto lookAt = glm::lookAt(viewPosition, glm::vec3(0), glm::vec3(0, 1, 0));
			auto viewMatrix = glm::rotate(glm::mat4(1), float(roll), glm::vec3(0, 0, 1)) * lookAt;

			auto fov = 60.0f;
			auto aspect = float(width) / height;
			auto znear = 0.01f;
			auto zfar = 100.0f;
			auto projectionMatrix = glm::perspective(fov * float(M_PI / 180.0f), aspect, znear, zfar);

			auto offset = 0u;
			map<const Transform*, unsigned int> offsetMap;

			int sceneIndex = int(sync_get_val(sceneIndexTrack, row));
			sceneIndex = max(sceneIndex, 0);
			sceneIndex %= sceneRenderers.size();
			SceneRenderer &sceneRenderer = sceneRenderers[sceneIndex];

			sceneRenderer.draw(commandBuffer, viewMatrix, projectionMatrix);
			vkCmdEndRenderPass(commandBuffer);

			imageBarrier(
				commandBuffer,
				colorArray.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

			blitImage(commandBuffer,
				colorRenderTarget.getImage(),
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

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);

			imageBarrier(
				commandBuffer,
				computeRenderTarget.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			pushConstantData.arrayBufferFrame = uint32_t(arrayBufferFrame);
			pushConstantData.validFrames = uint32_t(validFrames);
			pushConstantData.delayImage = uint32_t(sync_get_val(pp_delay_image, row));
			pushConstantData.delayAmount = float(sync_get_val(pp_delay_amount, row));
			pushConstantData.delayChroma = float(1.0 - min(max(0.0, sync_get_val(pp_delay_chroma, row)), 1.0));

			vkCmdPushConstants(commandBuffer, computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstantData), &pushConstantData);

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

			blitImage(commandBuffer,
				computeRenderTarget.getImage(),
				images[currentSwapImage],
				width, height,
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 });

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
