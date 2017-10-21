#ifndef SCENE_H
#define SCENE_H

#include "texture.h"

#include <glm/glm.hpp>

#include <list>

enum VertexFormat {
	VERTEX_FORMAT_NONE = 0,
	VERTEX_FORMAT_POSITION = (1 << 0),
	VERTEX_FORMAT_NORMAL = (1 << 1),
	VERTEX_FORMAT_TANGENT = (1 << 2),
	VERTEX_FORMAT_BINORMAL = (1 << 3)
};

enum IndexType {
	INDEX_TYPE_UINT16,
	INDEX_TYPE_UINT32
};

inline VertexFormat operator|(const VertexFormat &a, const VertexFormat &b)
{
	return static_cast<VertexFormat>(static_cast<int>(a) | static_cast<int>(b));
}

inline VertexFormat operator |= (VertexFormat &a, const VertexFormat &b)
{
	return a = static_cast<VertexFormat>(static_cast<int>(a) | static_cast<int>(b));
}

class Mesh {
public:
	Mesh(const std::vector<uint8_t> &vertices, VertexFormat vertexFormat, const std::vector<uint8_t> &indices, IndexType indexType) :
		vertices(vertices),
		vertexFormat(vertexFormat),
		indices(indices),
		indexType(indexType)
	{
	}

	static size_t calculateVertexStride(VertexFormat vertexFormat)
	{
		size_t stride = 0;

		if (vertexFormat & VERTEX_FORMAT_POSITION)
			stride += sizeof(float) * 3;

		if (vertexFormat & VERTEX_FORMAT_NORMAL)
			stride += sizeof(float) * 3;

		if (vertexFormat & VERTEX_FORMAT_TANGENT)
			stride += sizeof(float) * 3;

		if (vertexFormat & VERTEX_FORMAT_BINORMAL)
			stride += sizeof(float) * 3;

		return stride;
	}

	const std::vector<uint8_t>& getVertices() const { return vertices; }
	VertexFormat getVertexFormat() const { return vertexFormat; }
	size_t getVertexStride() const { return calculateVertexStride(vertexFormat); }
	const std::vector<uint8_t>& getIndices() const { return indices; }
	IndexType getIndexType() const { return indexType; }

private:
	std::vector<uint8_t> vertices;
	VertexFormat vertexFormat;
	std::vector<uint8_t> indices;
	IndexType indexType;
};

class Material {
	Texture2D *albedoMap;
	glm::vec4 albedoColor;

	// TODO: these should be baked (shininess)
	Texture2D *normalMap;
	Texture2D *specularMap;
};

class Model {
public:
	Model(const Mesh *mesh, const Material *material) :
		mesh(mesh),
		material(material)
	{
		assert(mesh != nullptr);
		assert(material != nullptr);
	}

	const Mesh *getMesh() const { return mesh; }
	const Material *getMaterial() const { return material; }

private:
	const Mesh *mesh;
	const Material *material;
};

class Transform {
public:
	Transform() : parent(nullptr)
	{
	}

	virtual ~Transform()
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

class RootTransform : public Transform {
public:
	RootTransform() : Transform()
	{
	}

	glm::mat4 getLocalMatrix() const override
	{
		return glm::mat4(1);
	}
};

class MatrixTransform : public Transform {
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

class Object {
public:
	Object(Model *model, Transform *transform) :
		model(model),
		transform(transform)
	{
		assert(model != nullptr);
		assert(transform != nullptr);
	}

	const Model *getModel() const { return model; }
	const Transform *getTransform() const { return transform; }

private:
	Model *model;
	Transform *transform;
};

class Scene {
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

struct IndexedBatch
{
public:
	IndexedBatch(const std::vector<VkBuffer> &vertexBuffers, const std::vector<VkDeviceSize> &vertexBufferOffsets, VkBuffer indexBuffer, VkIndexType indexType, int indexCount) :
		vertexBuffers(vertexBuffers),
		vertexBufferOffsets(vertexBufferOffsets),
		indexBuffer(indexBuffer),
		indexType(indexType),
		indexCount(indexCount)
	{
		assert(vertexBuffers.size() == vertexBufferOffsets.size());
	}

	void bind(VkCommandBuffer commandBuffer)
	{
		vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), vertexBufferOffsets.data());
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, indexType);
	}

	void draw(VkCommandBuffer commandBuffer)
	{
		vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
	}

private:
	std::vector<VkBuffer> vertexBuffers;
	std::vector<VkDeviceSize> vertexBufferOffsets;
	VkBuffer indexBuffer;
	VkIndexType indexType;
	uint32_t indexCount;
};

std::vector<VkVertexInputAttributeDescription> vertexFormatToInputAttributeDescriptions(VertexFormat vertexFormat);
IndexedBatch meshToIndexedBatch(const Mesh &mesh);

#endif // SCENE_H
