namespace vulkan
{
	extern VkInstance instance;
	extern VkDevice device;
	extern VkPhysicalDevice physicalDevice;
	extern VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	extern VkQueue graphicsQueue;

	extern VkDebugReportCallbackEXT debugReportCallback;

	VkResult init(const char *appName);

	extern struct device_funcs {
		PFN_vkQueuePresentKHR vkQueuePresentKHR;
	} deviceFuncs;

	void deviceFuncsInit(VkDevice device);

	extern struct instance_funcs {
		PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;

		PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
		PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
	} instanceFuncs;

	void instanceFuncsInit(VkInstance instance);
};
