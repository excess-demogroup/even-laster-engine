#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 texCoord;
layout (location = 0) out vec4 outFragColor;

layout (binding = 0) uniform sampler2D colorSampler;
layout (binding = 1) uniform sampler2D bloomSampler;

layout(push_constant) uniform PushConstants {
	float bloomAmount;
	float bloomShape;
	float seed;
} pushConstants;


float nrand(vec2 n)
{
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 sampleBloom(vec2 pos, float shape)
{
	vec3 sum = vec3(0);
	int levels = textureQueryLevels(bloomSampler);
	float total = 0;
	for (int i = 0; i < levels; ++i) {
		float weight = pow(float(i), shape);
		vec2 rnd = vec2(nrand(3 + i + pos.xy + pushConstants.seed),
		                nrand(5 + i + pos.yx - pushConstants.seed));
		rnd = (rnd * 2 - 1) / textureSize(bloomSampler, i);
		sum += textureLod(bloomSampler, pos + rnd * 0.25, float(i)).rgb * weight;
		total += weight;
	}
	return sum / total;
}

void main()
{
	vec3 color = texture(colorSampler, texCoord).rgb;
	color += sampleBloom(texCoord, pushConstants.bloomShape) * pushConstants.bloomAmount;

	outFragColor = vec4(color, 1);
}
