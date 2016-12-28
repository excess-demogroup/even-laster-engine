#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inPos;

layout (binding = 0) uniform UBO
{
	mat4 modelViewProjectionMatrix;
} ubo;

layout (location = 0) out vec3 outColor;

void main()
{
	outColor = 0.5f + 0.5f * inPos.xyz;
	gl_Position = ubo.modelViewProjectionMatrix * vec4(inPos.xyz, 1.0);
}
