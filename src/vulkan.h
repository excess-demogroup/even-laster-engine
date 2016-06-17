namespace vulkan
{
	extern VkInstance instance;
	extern VkDevice device;
	extern VkPhysicalDevice physicalDevice;
	extern VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	extern VkQueue graphicsQueue;
	extern int graphicsQueueIndex;

	extern VkDebugReportCallbackEXT debugReportCallback;

	VkResult init(const char *appName, const std::vector<const char *> &enabledExtensions);

	extern struct instance_funcs {
		PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
		PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
		PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
	} instanceFuncs;

	void instanceFuncsInit(VkInstance instance);
};
