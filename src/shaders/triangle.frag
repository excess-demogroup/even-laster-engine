#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec2 texCoord;
layout (location = 1) in vec3 modelPos;

layout (location = 0) out vec4 outFragColor;

layout (binding = 0) uniform UBO
{
	mat4 modelViewProjectionMatrix;
	vec4 viewPosition;
} ubo;

layout (binding = 1) uniform sampler2D samplerColor;

vec2 trace(vec3 pos, vec3 dir)
{
	float dist = 0;
	float t = -(pos.z + dist) / dir.z;
	pos += dir * t;
	return (pos.xy + 1) * 0.5;
}

vec3 spectrum_offset(float t)
{
	// Thanks to mentor! https://twitter.com/Stubbesaurus/status/818847844790575104
	float t0 = 3.0 * t - 1.5;
	return clamp( vec3( -t0, 1.0-abs(t0), t0), 0.0, 1.0);
}

float nrand( vec2 n )
{
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233)))* 43758.5453);
}

vec3 sampleSpectrum(vec2 uvA, vec2 uvB)
{
	const int num_iter = 7;
	const float stepsiz = 1.0 / (float(num_iter)-1.0);

	float rnd = nrand(uvA.xy);
	float t = rnd * stepsiz;

	vec3 sumcol = vec3(0.0);
	vec3 sumw = vec3(0.0);
	for (int i = 0; i < num_iter; ++i)
	{
		vec3 w = spectrum_offset(t);
		sumw += w;

		vec2 uv = mix(uvA, uvB, t);
		vec3 color = textureLod(samplerColor, uv, 0).xyz;

		sumcol += w * color;
		t += stepsiz;
	}
	return sumcol.rgb /= sumw;
}

void main()
{
	vec3 modelNormal = normalize(cross(dFdx(modelPos), dFdy(modelPos)));
	vec3 pos = modelPos;
	vec3 viewPos = ubo.viewPosition.xyz;
	vec3 view = normalize(modelPos - viewPos);

	float refractiveIndex = 1.1;
	vec3 dirR = refract(view, modelNormal, 1.0 / (refractiveIndex + 0.025));
	vec3 dirB = refract(view, modelNormal, 1.0 / (refractiveIndex + 0.075));

	vec2 hitA = trace(pos, dirR);
	vec2 hitB = trace(pos, dirB);

	vec3 color = sampleSpectrum(hitA, hitB);
	color += pow(1 - dot(modelNormal, -view), 3) * 0.5;
	outFragColor = vec4(color, 1);
}
