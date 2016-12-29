#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 outFragColor;

layout (binding = 1) uniform sampler2D samplerColor;

void main()
{
    outFragColor = vec4(textureLod(samplerColor, inColor.xy, 0.25).xyz * inColor, 1.0);
}
