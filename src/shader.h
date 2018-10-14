#ifndef SHADER_H
#define SHADER_H

#include "vkinstance.h"

VkShaderModule loadShaderModule(const std::string &path);

class ShaderStage {
public:
	ShaderStage(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule) :
		shaderStage(shaderStage),
		shaderModule(shaderModule)
	{
	}

	VkPipelineShaderStageCreateInfo getPipelineShaderStageCreateInfo() const
	{
		VkPipelineShaderStageCreateInfo ret = {};
		ret.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		ret.stage = shaderStage;
		ret.module = shaderModule;
		ret.pName = "main";
		return ret;
	}

private:
	VkShaderStageFlagBits shaderStage;
	VkShaderModule shaderModule;
};

class ShaderDescriptor {
public:
	ShaderDescriptor(int binding, VkDescriptorType descriptorType, int count, VkShaderStageFlags stageFlags, const std::vector<VkSampler> &immutableSamplers = {}) :
		binding(binding),
		descriptorType(descriptorType),
		count(count),
		stageFlags(stageFlags),
		immutableSamplers(immutableSamplers)
	{
		assert(count > 0);
		assert(immutableSamplers.size() == 0 || immutableSamplers.size() == count);
	}

	VkDescriptorSetLayoutBinding getBinding()
	{
		VkDescriptorSetLayoutBinding ret;
		ret.binding = uint32_t(binding);
		ret.descriptorType = descriptorType;
		ret.descriptorCount = uint32_t(count);
		ret.stageFlags = stageFlags;
		ret.pImmutableSamplers = immutableSamplers.size() > 0 ? immutableSamplers.data() : nullptr;
		return ret;
	}

private:
	int binding;
	VkDescriptorType descriptorType;
	int count;
	VkShaderStageFlags stageFlags;
	const std::vector<VkSampler> immutableSamplers;
};


class ShaderProgram {
public:
	ShaderProgram(const std::vector<ShaderStage> &stages, const std::vector<ShaderDescriptor> &descriptors, const std::vector<VkPushConstantRange> &pushConstantRanges = {}) :
		stages(stages)
	{
		std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
		layoutBindings.reserve(descriptors.size());
		for (auto descriptor : descriptors)
			layoutBindings.push_back(descriptor.getBinding());

		descriptorSetLayout = vulkan::createDescriptorSetLayout(layoutBindings);
		pipelineLayout = vulkan::createPipelineLayout({ descriptorSetLayout }, pushConstantRanges);
	}

	VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }
	VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

	std::vector<VkPipelineShaderStageCreateInfo> getPipelineShaderStageCreateInfos() const
	{
		std::vector<VkPipelineShaderStageCreateInfo> ret;
		ret.reserve(stages.size());
		for (auto stage : stages)
			ret.push_back(stage.getPipelineShaderStageCreateInfo());

		return ret;
	}

private:
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	const std::vector<ShaderStage> stages;
};

#endif /* SHADER_H */
