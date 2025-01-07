// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

struct FrameData
{
	VkCommandPool _commandPool;
	VkCommandBuffer _commandBuffer;
	// 添加交换链信号量，当交换链图像可用时，交换链信号量会发出信号；
	// 添加渲染信号量，当渲染完成时，渲染信号量会发出信号；
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	// 添加栅栏，当栅栏发出信号时，所有在栅栏上等待的命令缓冲区都会执行；
	VkFence _renderFence;
};

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 3;

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

	/**
	 * @brief 获取当前帧数据
	 * @return FrameData& 当前帧数据
	 */
	FrameData& get_current_frame()
	{
		return _frames[_currentFrame % MAX_FRAMES_IN_FLIGHT];
	}

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
	// vulkan 对象
	VkInstance _instance;
	// 调试消息	
	VkDebugUtilsMessengerEXT _debugMessenger;
	// 物理设备
	VkPhysicalDevice _chosenGPU;
	// 逻辑设备
	VkDevice _device;
	// 表面，用于展示渲染结果
	VkSurfaceKHR _surface;

	// 交换链，用于管理渲染图像
	VkSwapchainKHR _swapchain;
	// 交换链图像格式
	VkFormat _swapchainImageFormat;
	// 交换链图像
	std::vector<VkImage> _swapchainImages;
	// 交换链图像视图
	std::vector<VkImageView> _swapchainImageViews;

	// 交换链图像大小
	VkExtent2D _swapchainExtent;

	// 帧数据
	std::array<FrameData, MAX_FRAMES_IN_FLIGHT> _frames;
	// 当前帧
	uint32_t _currentFrame = 0;

	// 图形队列
	VkQueue _graphicsQueue;
	// 图形队列族
	uint32_t _graphicsQueueFamily;
};
