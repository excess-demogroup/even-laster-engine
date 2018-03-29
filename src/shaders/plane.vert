#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec3 outModelPos;

layout (binding = 0) uniform UBO
{
	mat4 modelViewMatrix;
	mat4 modelViewInverseMatrix;
	mat4 modelViewProjectionMatrix;
	vec2 offset;
	vec2 scale;
	float time;
} ubo;

layout (binding = 1) uniform sampler3D volumeSampler;

void main()
{
	int strip = gl_VertexIndex >> 16;
	int stripIndex = gl_VertexIndex & 0xFFFF;
	vec2 texCoord = vec2(stripIndex >> 1, strip + (stripIndex & 1));
	texCoord = (texCoord - 127.5) / 128;
	texCoord *= 0.1;
	vec3 position = vec3(ubo.offset + ubo.scale * texCoord, ubo.time);

	position = textureLod(volumeSampler, position, 0).xyz;
	position += textureLod(volumeSampler, position * 3, 0).xyz * 0.5;
	position += textureLod(volumeSampler, position * 15, 0).xyz * 0.01;
	position *= 5;

	outTexCoord = texCoord;
	outModelPos = position;
	gl_Position = ubo.modelViewProjectionMatrix * vec4(position, 1.0);
}
