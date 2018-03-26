#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform UBO
{
	mat4 modelViewMatrix;
	mat4 modelViewProjectionMatrix;
	mat4 modelViewProjectionInverseMatrix;
	vec4 viewPosition;
} ubo;

layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec3 outModelPos;

void main()
{
	outModelPos = inPos;
	outTexCoord = 0.5 + 0.5 * inPos.xy;
	gl_Position = ubo.modelViewProjectionMatrix * vec4(inPos.xyz, 1.0);
}
