#ifndef SHADER_H
#define SHADER_H

#include "vulkan.h"

VkPipelineShaderStageCreateInfo loadShader(const char *fileName, VkDevice device, VkShaderStageFlagBits stage, const char *name = "main");

#endif /* SHADER_H */
