#ifndef CORE_H
#define CORE_H

#ifdef _MSC_VER
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

#endif // CORE_H
