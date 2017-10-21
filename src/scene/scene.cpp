#include "../vulkan.h"
#include "../core/core.h"
#include "../core/blobbuilder.h"

#include "scene.h"

#include <vector>

using std::vector;

IndexedBatch meshToIndexedBatch(const Mesh &mesh)
{
	auto vertices = mesh.getVertices();
	auto indices = mesh.getIndices();

	auto vertexStagingBuffer = new StagingBuffer(vertices.size());
	vertexStagingBuffer->uploadMemory(0, vertices.data(), vertices.size());

	auto vertexBuffer = new Buffer(vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	vertexBuffer->uploadFromStagingBuffer(vertexStagingBuffer, 0, 0, vertices.size());

	auto indexBuffer = new Buffer(indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	indexBuffer->uploadMemory(0, indices.data(), indices.size());

	VkIndexType indexType;
	int indexCount;
	switch (mesh.getIndexType()) {
	case INDEX_TYPE_UINT16:
		indexType = VK_INDEX_TYPE_UINT16;
		indexCount = indices.size() / sizeof(uint16_t);
		break;

	case INDEX_TYPE_UINT32:
		indexType = VK_INDEX_TYPE_UINT32;
		indexCount = indices.size() / sizeof(uint32_t);
		break;

	default:
		unreachable("invalid index-type!");
	}

	// FIXME: leaks both vertexBuffer and indexBuffer!

	return IndexedBatch(
		std::vector<VkBuffer> { vertexBuffer->getBuffer() },
		std::vector<VkDeviceSize> { 0 },
		indexBuffer->getBuffer(),
		indexType,
		indexCount);
}

vector<VkVertexInputAttributeDescription> vertexFormatToInputAttributeDescriptions(VertexFormat vertexFormat)
{
	vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
	int offset = 0;

	if (vertexFormat & VERTEX_FORMAT_POSITION) {
		VkVertexInputAttributeDescription attr;
		attr.binding = 0;
		attr.location = 0;
		attr.format = VK_FORMAT_R32G32B32_SFLOAT;
		attr.offset = offset;
		vertexInputAttributeDescriptions.push_back(attr);
		offset += sizeof(float) * 3;
	}

	if (vertexFormat & VERTEX_FORMAT_NORMAL) {
		VkVertexInputAttributeDescription attr;
		attr.binding = 0;
		attr.location = 1;
		attr.format = VK_FORMAT_R32G32B32_SFLOAT;
		attr.offset = offset;
		vertexInputAttributeDescriptions.push_back(attr);
		offset += sizeof(float) * 3;
	}

	if (vertexFormat & VERTEX_FORMAT_TANGENT) {
		VkVertexInputAttributeDescription attr;
		attr.binding = 0;
		attr.location = 2;
		attr.format = VK_FORMAT_R32G32B32_SFLOAT;
		attr.offset = offset;
		vertexInputAttributeDescriptions.push_back(attr);
		offset += sizeof(float) * 3;
	}

	if (vertexFormat & VERTEX_FORMAT_BINORMAL) {
		VkVertexInputAttributeDescription attr;
		attr.binding = 0;
		attr.location = 3;
		attr.format = VK_FORMAT_R32G32B32_SFLOAT;
		attr.offset = offset;
		vertexInputAttributeDescriptions.push_back(attr);
		offset += sizeof(float) * 3;
	}

	return vertexInputAttributeDescriptions;
}
