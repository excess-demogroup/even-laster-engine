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

		ColorRenderTarget colorRenderTarget(VK_FORMAT_R16G16B16A16_SFLOAT, width, height, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
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
		sceneColorAttachment.format = colorRenderTarget.getFormat();
		sceneColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		sceneColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		sceneColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		sceneColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		sceneColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		sceneColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		sceneColorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
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
			{ depthRenderTarget.getImageView(), colorRenderTarget.getImageView() },
			sceneRenderPass);

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

		auto texture = importTexture2D("assets/excess-logo.png", TextureImportFlags::GENERATE_MIPMAPS);
		auto offsetMaps = importTexture2DArray("assets/offset-maps", TextureImportFlags::NONE);

		VkSampler textureSampler = createSampler(float(texture.getMipLevels()), false, false);
		VkDescriptorImageInfo descriptorImageInfo = texture.getDescriptorImageInfo(textureSampler);

		struct {
			float fade;
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

		VkSampler arrayTextureSampler = createSampler(1.0f, false, false);

		auto postProcessDescriptorSetLayout = createDescriptorSetLayout({
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
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
		}, swapChain.getImageViews().size());

		auto postProcessDescriptorSet = allocateDescriptorSet(postProcessDescriptorPool, postProcessDescriptorSetLayout);
		{
			VkDescriptorImageInfo postProcessRenderTargetImageInfo = {};
			postProcessRenderTargetImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			postProcessRenderTargetImageInfo.imageView = postProcessRenderTarget.getImageView();

			VkWriteDescriptorSet writeDescriptorSets[3] = {};
			writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[0].dstSet = postProcessDescriptorSet;
			writeDescriptorSets[0].dstBinding = 0;
			writeDescriptorSets[0].descriptorCount = 1;
			writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDescriptorSets[0].pImageInfo = &postProcessRenderTargetImageInfo;

			VkDescriptorImageInfo descriptorImageInfo1 = {};
			descriptorImageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfo1.imageView = colorArray.getImageView();
			descriptorImageInfo1.sampler = arrayTextureSampler;

			writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[1].dstSet = postProcessDescriptorSet;
			writeDescriptorSets[1].dstBinding = 1;
			writeDescriptorSets[1].descriptorCount = 1;
			writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[1].pImageInfo = &descriptorImageInfo1;

			VkDescriptorImageInfo descriptorImageInfo2 = {};
			descriptorImageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			descriptorImageInfo2.imageView = offsetMaps.getImageView();
			descriptorImageInfo2.sampler = arrayTextureSampler;

			writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSets[2].dstSet = postProcessDescriptorSet;
			writeDescriptorSets[2].dstBinding = 2;
			writeDescriptorSets[2].descriptorCount = 1;
			writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSets[2].pImageInfo = &descriptorImageInfo2;

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

		auto clear_r = sync_get_track(rocket, "background:clear.r");
		auto clear_g = sync_get_track(rocket, "background:clear.g");
		auto clear_b = sync_get_track(rocket, "background:clear.b");

		auto cam_rot = sync_get_track(rocket, "camera:rot.y");
		auto cam_dist = sync_get_track(rocket, "camera:dist");
		auto cam_roll = sync_get_track(rocket, "camera:roll");

		auto refractionFadeTrack = sync_get_track(rocket, "refraction:fade");

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

			refractionUniforms.fade = float(sync_get_val(refractionFadeTrack, row));
			auto ptr = refractionUniformBuffer->map(0, sizeof(refractionUniforms));
			memcpy(ptr, &refractionUniforms, sizeof(refractionUniforms));
			refractionUniformBuffer->unmap();

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

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, postProcessPipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, postProcessPipelineLayout, 0, 1, &postProcessDescriptorSet, 0, nullptr);

			imageBarrier(
				commandBuffer,
				postProcessRenderTarget.getImage(),
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0, VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			postProcessPushConstantData.arrayBufferFrame = uint32_t(arrayBufferFrame);
			postProcessPushConstantData.validFrames = uint32_t(validFrames);
			postProcessPushConstantData.delayImage = uint32_t(sync_get_val(pp_delay_image, row));
			postProcessPushConstantData.delayAmount = float(sync_get_val(pp_delay_amount, row));
			postProcessPushConstantData.delayChroma = float(1.0 - min(max(0.0, sync_get_val(pp_delay_chroma, row)), 1.0));

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
