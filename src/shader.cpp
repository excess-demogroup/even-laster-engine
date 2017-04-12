#include "shader.h"
#include "core/memorymappedfile.h"

static VkShaderModule loadShaderModule(const char *path, VkDevice device, VkShaderStageFlagBits stage)
{
    MemoryMappedFile shaderCode(path);
    assert(shaderCode.getSize() > 0);

    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = shaderCode.getSize();
    moduleCreateInfo.pCode = static_cast<const uint32_t *>(shaderCode.getData());

    VkShaderModule shaderModule;
    VkResult err = vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule);
    assert(!err);

    return shaderModule;
}

VkPipelineShaderStageCreateInfo loadShader(const char *fileName, VkDevice device, VkShaderStageFlagBits stage, const char *name)
{
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
    shaderStage.module = loadShaderModule(fileName, device, stage);
    shaderStage.pName = name;
    return shaderStage;
}
