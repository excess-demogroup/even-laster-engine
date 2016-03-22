#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inPos;

/*
layout (binding = 0) uniform UBO
{
	mat4 projectionMatrix;
	mat4 modelViewMatrix;
	mat4 modelViewProjectionMatrix;
} ubo;
*/

layout (location = 0) out vec3 outColor;

void main()
{
	outColor = vec3(1.0, 0.0, 1.0);
	gl_Position = vec4(inPos.xyz, 1.0);
/*	gl_Position = ubo.projectionMatrix * ubo.viewMatrix * ubo.modelMatrix * vec4(inPos.xyz, 1.0); */
}
