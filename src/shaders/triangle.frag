#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 texCoord;

layout (location = 0) out vec4 outFragColor;

layout (binding = 1) uniform sampler2D samplerColor;

void main()
{
	outFragColor = vec4(textureLod(samplerColor, texCoord, 0.35).xyz, 1.0);
}
