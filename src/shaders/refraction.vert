#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec3 inPos;

#include "perobject.glsl"

layout (location = 0) out vec2 outTexCoord;
layout (location = 1) out vec3 outModelPos;

void main()
{
	outModelPos = inPos;
	outTexCoord = 0.5 + 0.5 * inPos.xy;
	gl_Position = perObjectUBO.modelViewProjectionMatrix * vec4(inPos.xyz, 1.0);
}
