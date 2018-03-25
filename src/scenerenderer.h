#ifndef SCENERENDERER_H
#define SCENERENDERER_H

#include "vulkan.h"
#include "scene/scene.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <map>

class Scene;
class Mesh;
class Buffer;

class SceneRenderer {
public:

	SceneRenderer(Scene *scene, VkRenderPass renderPass);
	void draw(VkCommandBuffer commandBuffer, const glm::mat4 &viewProjectionMatrix);

private:
	Scene *scene;

	VkPipelineLayout pipelineLayout;
	VkDescriptorSet descriptorSet;
	std::map<const Mesh *, IndexedBatch> indexedBatches;
	std::map<VertexFormat, VkPipeline> pipelines;
	VkSampler textureSampler;

	Buffer *uniformBuffer;
	uint32_t uniformBufferSpacing;
};

#endif