float nrand(vec2 n)
{
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 spectrum_offset(float t)
{
	// Thanks to mentor! https://twitter.com/Stubbesaurus/status/818847844790575104
	float t0 = 3.0 * t - 1.5;
	return clamp( vec3( -t0, 1.0-abs(t0), t0), 0.0, 1.0);
}

uint rand(uint seed)
{
	int a = 1103515245;
	int c = 12345;
	seed = (a * seed + c) & 0xFFFFFFFF;
	return seed;
}

float randf(uint seed)
{
	uint r = rand(seed);
	return float(r >> (32 - 23)) / ((1 << 23) - 1);
}

vec3 fromLinear(vec3 linearRGB)
{
    bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
    vec3 higher = vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower = linearRGB * vec3(12.92);

    return mix(higher, lower, cutoff);
}

vec3 toLinear(vec3 sRGB)
{
    bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
    vec3 higher = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
    vec3 lower = sRGB / vec3(12.92);

    return mix(higher, lower, cutoff);
}
