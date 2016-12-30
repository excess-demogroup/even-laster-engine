#ifndef CORE_H
#define CORE_H

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define snprintf _snprintf
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

#endif // CORE_H
