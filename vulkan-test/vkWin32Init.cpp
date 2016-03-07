#include <stdafx.h>

static VkSurfaceKHR surface;

void vkWin32Init(HINSTANCE hInstance, HWND hWnd, VkInstance instance)
{
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = hInstance;
	surfaceCreateInfo.hwnd = hWnd;
	VkResult err = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
}
