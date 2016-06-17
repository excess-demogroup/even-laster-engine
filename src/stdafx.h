#pragma once

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define snprintf _snprintf
#endif

#include <stdlib.h>
#include <assert.h>
#include <vector>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

#define VK_PROTOTYPES

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.h>
