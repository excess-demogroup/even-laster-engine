#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 texCoord;
layout (location = 0) out vec4 outFragColor;

layout (binding = 0) uniform sampler2D textureSampler;


void main()
{
	float centerWeight = 0.16210282163712664;
	vec2 diagonalOffsets = vec2(0.3842896354828526, 1.2048616327242379);
	vec4 offsets = vec4(-diagonalOffsets.xy, +diagonalOffsets.xy) / textureSize(textureSampler, 0).xyxy;
	float diagonalWeight = 0.2085034734347498;

	outFragColor = texture(textureSampler, texCoord) * centerWeight +
	               texture(textureSampler, texCoord + offsets.xy) * diagonalWeight +
	               texture(textureSampler, texCoord + offsets.wx) * diagonalWeight +
	               texture(textureSampler, texCoord + offsets.zw) * diagonalWeight +
	               texture(textureSampler, texCoord + offsets.yz) * diagonalWeight;
}
