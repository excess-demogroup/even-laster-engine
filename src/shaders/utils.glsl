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
