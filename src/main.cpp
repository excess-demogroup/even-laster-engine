// vulkan-test.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <algorithm>
#include <list>
#include <map>

#include "vulkan.h"
#include "memorymappedfile.h"
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

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkBuffer *buffer)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usageFlags;
	VkResult err = vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer);
	assert(err == VK_SUCCESS);
}

uint32_t getMemoryTypeIndex(const VkMemoryRequirements &memoryRequirements, VkMemoryPropertyFlags propertyFlags)
{
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	for (auto i = 0u; i < VK_MAX_MEMORY_TYPES; i++) {
		if (((memoryRequirements.memoryTypeBits >> i) & 1) == 1) {
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
				return i;
				break;
			}
		}
	}

	assert(false);
	// critical error!
	throw std::runtime_error("invalid memory type!");
}

VkDeviceMemory allocateDeviceMemory(const VkMemoryRequirements &memoryRequirements, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = getMemoryTypeIndex(memoryRequirements, propertyFlags);

	VkDeviceMemory deviceMemory;
	VkResult err = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &deviceMemory);
	assert(err == VK_SUCCESS);

	return deviceMemory;
}

VkDeviceMemory allocateAndBindBufferDeviceMemory(VkBuffer buffer, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

	auto deviceMemory = allocateDeviceMemory(memoryRequirements, propertyFlags);

	VkResult err = vkBindBufferMemory(device, buffer, deviceMemory, 0);
	assert(!err);

	return deviceMemory;
}

VkDeviceMemory allocateAndBindImageDeviceMemory(VkImage image, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, image, &memoryRequirements);

	auto deviceMemory = allocateDeviceMemory(memoryRequirements, propertyFlags);

	VkResult err = vkBindImageMemory(device, image, deviceMemory, 0);
	assert(!err);

	return deviceMemory;
}

void uploadMemory(VkDeviceMemory deviceMemory, VkDeviceSize offset, void *data, VkDeviceSize size)
{
	void *mappedUniformMemory = nullptr;
	VkResult err = vkMapMemory(device, deviceMemory, offset, size, 0, &mappedUniformMemory);
	assert(err == VK_SUCCESS);

	memcpy(mappedUniformMemory, data, (size_t)size);

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

class Buffer {
public:
	Buffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags)
	{
		createBuffer(size, usageFlags, &buffer);
		deviceMemory = allocateAndBindBufferDeviceMemory(buffer, memoryPropertyFlags);
	}

	~Buffer()
	{
		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, deviceMemory, nullptr);
	}

	void *map(VkDeviceSize size)
	{
		void *ret;
		VkResult err = vkMapMemory(device, deviceMemory, 0, size, 0, &ret);
		assert(err == VK_SUCCESS);
		return ret;
	}

	void unmap()
	{
		vkUnmapMemory(device, deviceMemory);
	}

	VkBuffer getBuffer()
	{
		return buffer;
	}

private:
	VkBuffer buffer;
	VkDeviceMemory deviceMemory;
};

class StagingBuffer : public Buffer {
public:
	StagingBuffer(VkDeviceSize size) : Buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
	}
};

class TextureBase
{
protected:
	TextureBase(VkFormat format, VkImageType imageType, VkImageViewType imageViewType, int width, int height, int depth, int mipLevels = 1, int arrayLayers = 1) :
		format(format),
		imageType(imageType),
		imageViewType(imageViewType),
		baseWidth(width),
		baseHeight(height),
		baseDepth(depth),
		stagingBuffer(nullptr),
		lockedMipLevel(-1),
		lockedArrayLayer(-1)
	{
		assert(width > 0);
		assert(height > 0);
		assert(depth > 0);
		assert(mipLevels > 0);
		assert(arrayLayers > 0);

		image = createImage(format, imageType, width, height, depth, mipLevels, arrayLayers,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
		);

		deviceMemory = allocateAndBindImageDeviceMemory(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = image;
		imageViewCreateInfo.viewType = imageViewType;
		imageViewCreateInfo.format = format;
		imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = arrayLayers;
		imageViewCreateInfo.subresourceRange.levelCount = mipLevels;

		VkResult err = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView);
		assert(err == VK_SUCCESS);
	}

	static int mipSize(int size, int mipLevel)
	{
		return std::max(size >> mipLevel, 1);
	}

public:
	void *lock(size_t size, int mipLevel = 0, int arrayLayer = 0)
	{
		assert(lockedMipLevel < 0);
		assert(lockedArrayLayer < 0);
		assert(stagingBuffer == nullptr);

		assert(mipLevel >= 0);
		assert(arrayLayer >= 0);

		VkImageSubresource subRes = {};
		subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subRes.mipLevel = mipLevel;
		subRes.arrayLayer = arrayLayer;

		stagingBuffer = new StagingBuffer(size);
		void *ret = stagingBuffer->map(size);

		lockedMipLevel = mipLevel;
		lockedArrayLayer = arrayLayer;

		return ret;
	}

	void unlock(int mipLevel = 0, int arrayLayer = 0)
	{
		assert(mipLevel == lockedMipLevel);
		assert(arrayLayer == lockedArrayLayer);
		assert(stagingBuffer != nullptr);

		stagingBuffer->unmap();

		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.baseArrayLayer = arrayLayer;
		copyRegion.imageSubresource.mipLevel = mipLevel;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset = { 0, 0, 0 };
		copyRegion.imageExtent.width = mipSize(baseWidth, mipLevel);
		copyRegion.imageExtent.height = mipSize(baseHeight, mipLevel);
		copyRegion.imageExtent.depth = mipSize(baseDepth, mipLevel);

		auto commandBuffer = allocateCommandBuffers(setupCommandPool, 1)[0];

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		VkResult err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		assert(err == VK_SUCCESS);

		VkImageMemoryBarrier imageBarrier = {};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

		imageBarrier.image = image;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	
		imageBarrier.dstQueueFamilyIndex = graphicsQueueIndex;
		imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		imageBarrier.srcQueueFamilyIndex = graphicsQueueIndex;
		imageBarrier.srcAccessMask = 0;
		imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange.baseMipLevel = mipLevel;
		imageBarrier.subresourceRange.baseArrayLayer = arrayLayer;
		imageBarrier.subresourceRange.levelCount = 1;
		imageBarrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageBarrier);
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer->getBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		err = vkEndCommandBuffer(commandBuffer);
		assert(err == VK_SUCCESS);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Submit draw command buffer
		err = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(err == VK_SUCCESS);

		lockedMipLevel = -1;
		lockedArrayLayer = -1;

		// TODO: delete memory once init is done!
		// delete stagingBuffer;
		stagingBuffer = nullptr;
	}

	VkImageView getImageView()
	{
		return imageView;
	}

protected:
	VkFormat format;
	VkImageType imageType;
	VkImageViewType imageViewType;
	int baseWidth, baseHeight, baseDepth;

	StagingBuffer *stagingBuffer;
	int lockedMipLevel, lockedArrayLayer;

	VkImage image;
	VkImageView imageView;
	VkDeviceMemory deviceMemory;

private:
	static VkImage createImage(VkFormat format, VkImageType imageType, int width, int height, int depth, int mipLevels, int arrayLayers, VkImageTiling tiling, VkImageUsageFlags usage)
	{
		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = imageType;
		imageCreateInfo.format = format;
		imageCreateInfo.extent = { width, height, depth };
		imageCreateInfo.mipLevels = mipLevels;
		imageCreateInfo.arrayLayers = arrayLayers;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = tiling;
		imageCreateInfo.usage = usage;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		// imageCreateInfo.queueFamilyIndexCount - only needed if imageCreateInfo.sharingMode == VK_SHARING_MODE_CONCURRENT
		// imageCreateInfo.pQueueFamilyIndices - only needed if imageCreateInfo.sharingMode == VK_SHARING_MODE_CONCURRENT
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //  VK_IMAGE_LAYOUT_PREINITIALIZED;

		VkImage ret;
		VkResult err = vkCreateImage(device, &imageCreateInfo, nullptr, &ret);
		assert(err == VK_SUCCESS);

		return ret;
	}

};

class Texture2D : public TextureBase
{
public:
	Texture2D(VkFormat format, int width, int height, int mipLevels = 1) :
		TextureBase(format, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, width, height, 1, mipLevels)
	{
	}
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal, tangent, binormal;
	glm::vec2 uv[8];
};

class Mesh
{
public:
	Mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices) :
		vertices(vertices),
		indices(indices)
	{
	}

	const std::vector<Vertex> getVertices() const { return vertices; }
	const std::vector<uint32_t> getIndices() const { return indices; }

private:
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

class Material
{
	Texture2D *albedoMap;
	glm::vec4 albedoColor;

	// TODO: these should be baked (shinyness)
	Texture2D *normalMap;
	Texture2D *specularMap;
};

class Model
{
public:
	Model(const Mesh *mesh, const Material *material) :
		mesh(mesh),
		material(material)
	{
	}

private:
	const Mesh *mesh;
	const Material *material;
};

class Transform
{
public:
	Transform() : parent(nullptr)
	{
	}

	void setParent(Transform *parent)
	{
		if (this->parent != nullptr) {
			this->parent = nullptr;
			unrooted();
		}

		this->parent = parent;

		if (parent != nullptr)
			rooted();
	}

	virtual glm::mat4 getAbsoluteMatrix() const
	{
		glm::mat4 absoluteMatrix = getLocalMatrix();

		const Transform *curr = getParent();
		while (nullptr != curr) {
			absoluteMatrix = curr->getLocalMatrix() * absoluteMatrix;
			curr = curr->getParent();
		}

		return absoluteMatrix;
	}

	const Transform *getRootTransform() const
	{
		const Transform *curr = this;
		while (nullptr != curr->parent)
			curr = curr->parent;

		return curr;
	}

	Transform *getParent() const { return parent; }
	virtual glm::mat4 getLocalMatrix() const = 0;

protected:
	virtual void rooted() {}
	virtual void unrooted() {}

private:
	Transform *parent;
};

class RootTransform : public Transform
{
public:
	RootTransform() : Transform()
	{
	}

	glm::mat4 getLocalMatrix() const override
	{
		return glm::mat4(1);
	}
};

class MatrixTransform : public Transform
{
public:
	MatrixTransform() : Transform()
	{
		localMatrix = glm::mat4(1);
	}

	glm::mat4 getLocalMatrix() const override
	{
		return localMatrix;
	}

	void setLocalMatrix(glm::mat4 localMatrix)
	{
		this->localMatrix = localMatrix;
	}

private:
	glm::mat4 localMatrix;
};

class Object
{
public:
	Object(Model *model, Transform *transform) :
		model(model),
		transform(transform)
	{
		assert(model != nullptr);
		assert(transform != nullptr);
	}

	const Transform *getTransform() const { return transform; }

private:
	Model *model;
	Transform *transform;
};

class Scene
{
public:
	Scene()
	{
		transforms.push_back(&rootTransform);
	}

	MatrixTransform *createMatrixTransform(Transform *parent = nullptr)
	{
		auto trans = new MatrixTransform();

		if (parent == nullptr)
			parent = &rootTransform;

		trans->setParent(parent);
		transforms.push_back(trans);
		return trans;
	}

	Object *createObject(Model *model, Transform *transform = nullptr)
	{
		if (transform == nullptr)
			transform = &rootTransform;

		assert(transform->getRootTransform() == &rootTransform);

		auto obj = new Object(model, transform);
		objects.push_back(obj);
		return obj;
	}

	const Transform *getRootTransform() const { return &rootTransform; }

	const std::list<Object*> &getObjects() const { return objects; }
	const std::list<Transform*> &getTransforms() const { return transforms; }

private:
	std::list<Transform*> transforms;
	std::list<Object*> objects;
	RootTransform rootTransform;
};

VkDeviceSize alignSize(VkDeviceSize value, VkDeviceSize alignment)
{
	return ((value + alignment - 1) / alignment) * alignment;
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

		glm::vec3 vertexData[] = {
			glm::vec3(1.0f, 1.0f, 0.0f),
			glm::vec3(-1.0f, 1.0f, 0.0f),
			glm::vec3(0.0f, -1.0f, 0.0f)
		};

		Vertex v = { };
		v.position = glm::vec3(0, 0, 0);
		std::vector<Vertex> vertices;
		for (int i = 0; i < ARRAYSIZE(vertexData); ++i) {
			v.position = vertexData[i];
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

		Texture2D texture(VK_FORMAT_R8G8B8A8_UNORM, 64, 64);
		{
			size_t size = 64 * 4 * 64;
			void *ptr = texture.lock(size);
			for (auto y = 0; y < 64; ++y) {
				auto *row = (uint8_t *)ptr + 64 * 4 * y;
				for (auto x = 0; x < 64; ++x) {
					uint8_t tmp = ((x ^ y) & 16) != 0 ? 0xFF : 0x00;
					row[x * 4 + 0] = 0x80 + (tmp >> 1);
					row[x * 4 + 1] = 0xFF - (tmp >> 1);
					row[x * 4 + 2] = 0x80 + (tmp >> 1);
					row[x * 4 + 3] = 0xFF;
				}
			}
			texture.unlock();
		}

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

		VkBuffer uniformBuffer;
		uint32_t uniformBufferSpacing = (uint32_t)alignSize(sizeof(float) * 4 * 4, physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);
		VkDeviceSize uniformBufferSize = uniformBufferSpacing * scene.getTransforms().size();
		createBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &uniformBuffer);

		VkDeviceMemory uniformDeviceMemory = allocateAndBindBufferDeviceMemory(uniformBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		VkDescriptorBufferInfo descriptorBufferInfo = {};
		descriptorBufferInfo.buffer = uniformBuffer;
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

		VkBuffer vertexBuffer;
		createBuffer(sizeof(vertexData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vertexBuffer);

		VkDeviceMemory vertexDeviceMemory = allocateAndBindBufferDeviceMemory(vertexBuffer, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		uploadMemory(vertexDeviceMemory, 0, vertexData, sizeof(vertexData));

		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkSemaphore backBufferSemaphore, presentCompleteSemaphore;
		err = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &backBufferSemaphore);
		assert(err == VK_SUCCESS);
		err = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore);
		assert(err == VK_SUCCESS);

		auto commandBuffers = allocateCommandBuffers(commandPool, imageViews.size());

		VkFenceCreateInfo fenceCreateInfo = {};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		auto commandBufferFences = new VkFence[imageViews.size()];
		for (auto i = 0u; i < imageViews.size(); ++i) {
			err = vkCreateFence(device, &fenceCreateInfo, nullptr, commandBufferFences + i);
			assert(err == VK_SUCCESS);
		}

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

			int offset = 0;
			std::map<const Transform*, int> offsetMap;
			for each (auto transform in scene.getTransforms()) {
				auto modelMatrix = transform->getAbsoluteMatrix();
				uploadMemory(uniformDeviceMemory, offset, glm::value_ptr(modelMatrix), sizeof(modelMatrix));
				offsetMap[transform] = offset;
				offset += uniformBufferSpacing;
			}

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
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
