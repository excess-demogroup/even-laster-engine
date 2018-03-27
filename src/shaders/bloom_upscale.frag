#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (location = 0) in vec2 texCoord;
layout (location = 0) out vec4 outFragColor;

layout (binding = 0) uniform sampler2D colorSampler;
layout (binding = 1) uniform sampler2D bloomSampler;

float nrand( vec2 n )
{
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 sampleBloom(vec2 pos)
{
	vec3 sum = vec3(0);
	int levels = textureQueryLevels(bloomSampler);
	float total = 0;
	for (int i = 0; i < levels; ++i) {
		float weight = pow(float(i), 0.5);
		vec2 rnd = vec2(nrand(3 + i + pos.xy),
		                nrand(5 + i + pos.yx));
		rnd = (rnd * 2 - 1) / textureSize(bloomSampler, i);
		sum += textureLod(bloomSampler, pos + rnd * 0.25, float(i)).rgb * weight;
		total += weight;
	}
	return sum / total;
}

void main()
{
	vec3 sum = vec3(0);
	int levels = textureQueryLevels(bloomSampler);
	float total = 0;
	for (int i = 0; i < levels; ++i) {
		float weight = pow(float(i), 0.5);
		sum += textureLod(bloomSampler, texCoord, float(i)).rgb * weight;
		total += weight;
	}

	vec3 color = texture(colorSampler, texCoord).rgb;
	color += sampleBloom(texCoord);

	outFragColor = vec4(color, 1);
}
