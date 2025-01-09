// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include "vk_descriptors.h"

// 删除队列，用于管理资源的生命周期
struct DeletionQueue
{
    std::vector<std::function<void()>> _deletionQueue;

    /**
     * @brief 添加一个函数到删除队列
     * @param function 函数
     */
    void push_function(std::function<void()>&& function)
    {
        _deletionQueue.push_back(function);
    }

    /**
     * @brief 执行删除队列中的所有函数
     */
    void flush()
    {
        for (auto it = _deletionQueue.rbegin(); it != _deletionQueue.rend(); ++it) {
            (*it)();
        }
        _deletionQueue.clear();
    }
};

// 帧数据，包含命令池、命令缓冲区、信号量、栅栏、删除队列
struct FrameData
{
	VkCommandPool _commandPool;
	VkCommandBuffer _commandBuffer;
	// 添加交换链信号量，当交换链图像可用时，交换链信号量会发出信号；
	// 添加渲染信号量，当渲染完成时，渲染信号量会发出信号；
	VkSemaphore _swapchainSemaphore, _renderSemaphore;
	// 添加栅栏，当栅栏发出信号时，所有在栅栏上等待的命令缓冲区都会执行；
	VkFence _renderFence;
	// 添加删除队列，用于管理资源的生命周期
	DeletionQueue _deletionQueue;
};

// 计算管线常量
struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

// 计算管线配置
struct ComputePipeline
{
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	
	ComputePushConstants pushConstants;
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

	/**
	 * @brief 立即提交命令	
	 * @param function 函数
	 */
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

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

	/**
	 * @brief 初始化描述符

	 */
	void init_descriptors();

	/**
	 * @brief 初始化管线
	 */
	void init_pipelines();

	/**
	 * @brief 初始化背景管线
	 */
	void init_background_pipelines();
	
	/**
	 * @brief 初始化三角形管线
	 */
	void init_triangle_pipeline();

	/**
	 * @brief 初始化imgui
	 */
	void init_imgui();

	/**
	 * @brief 绘制背景
	 * @param cmd 命令缓冲区
	 */
	void draw_background(VkCommandBuffer cmd);
	
	/**
	 * @brief 绘制几何体
	 * @param cmd 命令缓冲区
	 */
	void draw_geometry(VkCommandBuffer cmd);
	
	/**
	 * @brief 绘制imgui
	 * @param cmd 命令缓冲区
	 * @param targetImageView 目标图像视图
	 */
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

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

	// 内存分配器
	VmaAllocator _allocator;

	// 删除队列
	DeletionQueue _mainDeletionQueue;

	// 分配的图像
	AllocatedImage _drawImage;
	// 分配的图像大小
	VkExtent2D _drawImageExtent;

	// 描述符分配器
	DescriptorAllocator _globalDescriptorAllocator;

	// 描述符集
	VkDescriptorSet _drawImageDescriptors;
	// 描述符集布局
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	// imgui 相关
	// 栅栏	
	VkFence _immFence;
	// 命令池
	VkCommandPool _immCmdPool;
	// 命令缓冲区
	VkCommandBuffer _immCmdBuffer;
	
	// 渲染管线相关
	// 渐变管线
	VkPipeline _gradientPipeline;
	// 渐变管线布局
	VkPipelineLayout _gradientPipelineLayout;

	// 多个计算管线
	std::vector<ComputePipeline> _backgroundPipelines;
	// 当前计算管线
	int _currentBackgroundPipeline{0};
	
	// 三角形管线
	VkPipeline _trianglePipeline;
	// 三角形管线布局
	VkPipelineLayout _trianglePipelineLayout;
};
