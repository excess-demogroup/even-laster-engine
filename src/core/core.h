#ifndef CORE_H
#define CORE_H

#include <stdint.h>

#if defined(_MSC_VER) && _MSC_VER < 1900
#define _CRT_SECURE_NO_WARNINGS
#define snprintf _snprintf
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

#ifdef _MSC_VER
#define unreachable(str) \
do { \
	assert(!str); \
	__assume(0); \
} while (0);
#else
#define unreachable(str) assert(!str)
#endif

#ifdef _MSC_VER
#include <intrin.h>
static inline uint32_t clz(uint32_t x)
{
	unsigned long r = 0;
	if (_BitScanReverse(&r, x))
		return 31 - r;
	else
		return 32;
}
#elif defined(__GNUC__)
static inline uint32_t clz(uint32_t x)
{
	return x ? __builtin_clz(x) : 32;
}
#else
static inline uint32_t clz(uint32_t x)
{
	unsigned n = 0;
	for (int i = 1; i < 32; i++) {
		if (x & (1 << 31))
			return n;
		n++;
		x <<= 1;
	}
	return n;
}
#endif

#endif // CORE_H
