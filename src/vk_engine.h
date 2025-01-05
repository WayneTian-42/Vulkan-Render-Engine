// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

class VulkanEngine 
{
public:

	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	/**
	 * @brief 单例模式，获取VulkanEngine对象
	 * @return VulkanEngine& VulkanEngine对象
	 */
	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:
	/**
	 * @brief 初始化Vulkan
	 */
	void init_vulkan();

	/**
	 * @brief 初始化交换链
	 */
	void init_swapchain();

	/**
	 * @brief 初始化command pool
	 */
	void init_commands();

	/**
	 * @brief 初始化同步结构
	 */
	void init_sync_structures();

	/**
	 * @brief 创建交换链
	 */
	void create_swapchain(uint32_t width, uint32_t height);

	/**
	 * @brief 销毁交换链
	 */
	void destroy_swapchain();

private:
	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debugMessenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;
};
