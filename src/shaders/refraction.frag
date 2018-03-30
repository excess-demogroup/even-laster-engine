#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "utils.glsl"
#include "perobject.glsl"

layout (location = 0) in vec2 texCoord;
layout (location = 1) in vec3 modelPos;

layout (location = 0) out vec4 outFragColor;

layout (binding = 1) uniform sampler2DArray samplerPlane;
layout (binding = 2) uniform samplerCube samplerEnv;

layout (binding = 3) uniform UBO
{
	float planeIndex;
	float fade;
	float refractiveIndex;
} ubo;

bool trace(vec3 pos, vec3 dir, out vec2 hit)
{
	float dist = 0;
	float t = -(pos.z + dist) / dir.z;
	if (t > 0.0) {
		pos += dir * t;
		hit = (pos.xy + 1) * 0.5;
		return true;
	} else
		return false;
}

vec3 sampleSpectrum(vec2 uvA, vec2 uvB)
{
	// thanks to Hornet ;)
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
		vec3 color = textureLod(samplerPlane, vec3(uv, ubo.planeIndex), 0).xyz;

		sumcol += w * color;
		t += stepsiz;
	}
	return sumcol.rgb /= sumw;
}

void main()
{
	vec3 modelNormal = normalize(cross(dFdx(modelPos), dFdy(modelPos)));
	vec3 pos = modelPos;
	vec3 viewPos = perObjectUBO.modelViewInverseMatrix[3].xyz;
	vec3 view = normalize(modelPos - viewPos);

	vec3 dirR = refract(view, modelNormal, 1.0 / (ubo.refractiveIndex + 0.025));
	vec3 dirB = refract(view, modelNormal, 1.0 / (ubo.refractiveIndex + 0.075));

	vec2 hitA, hitB;
	vec3 color = vec3(0);
	if (trace(pos, dirR, hitA) && trace(pos, dirB, hitB))
		color += sampleSpectrum(hitA, hitB) * ubo.fade;

	float fres = pow(1 - dot(modelNormal, -view), 3);
#if 1
	vec3 envCoord = reflect(normalize(view), modelNormal);
	color += (0.25 + 0.5 * texture(samplerEnv, envCoord).rgb * pow(1 - dot(modelNormal, -view), 3)) * fres;
#else
	color += fres * 0.5;
#endif

	outFragColor = vec4(color, 1);
}
