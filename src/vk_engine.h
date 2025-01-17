// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "vk_materials.h"
#include "camera.h"

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

	// 添加描述符分配器，用于管理描述符集的生命周期
	DescriptorAllocatorGrowable _frameDescriptorAllocator;
};

struct GPUSceneData {
	glm::mat4 view;	
	glm::mat4 proj;
	glm::mat4 viewProj;
	glm::vec4 ambientColor;
	glm::vec4 lightDirection;
	glm::vec4 lightColor;
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
	VkExtent2D _windowExtent{ 1600 , 800 };

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
	
	/**
	 * @brief 创建网格并上传到GPU
	 * @param indices 索引
	 * @param vertices 顶点
	 * @return GPUMeshBuffers 网格缓冲区
	 */
	GPUMeshBuffers upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	/**
	 * @brief 获取设备
	 * @return VkDevice 设备
	 */
	VkDevice get_device() const { return _device; }

	/**
	 * @brief 获取场景数据描述符集布局
	 * @return VkDescriptorSetLayout 场景数据描述符集布局
	 */
	VkDescriptorSetLayout get_scene_set_layout() const { return _sceneDataDescriptorLayout; }

	/**
	 * @brief 获取绘制图像格式
	 * @return VkFormat 绘制图像格式
	 */
	VkFormat get_draw_image_format() const { return _drawImage.imageFormat;}

	/**
	 * @brief 获取深度图像格式
	 * @return VkFormat 深度图像格式
	 */
	VkFormat get_depth_image_format() const { return _depthImage.imageFormat; }

	/**
	 * @brief 获取错误棋盘图像
	 * @return AllocatedImage 错误棋盘图像
	 */
	AllocatedImage get_error_image() const { return _errorCheckerboardImage; }

	/**
	 * @brief 获取白色图像
	 * @return AllocatedImage 白色图像
	 */
	AllocatedImage get_white_image() const { return _whiteImage; }

	/**
	 * @brief 获取线性采样器
	 * @return VkSampler 线性采样器
	 */
	VkSampler get_sampler_linear() const { return _defaultSamplerLinear; }

	/**
	 * @brief 创建缓冲区
	 * @param allocSize 分配大小
	 * @param usage 缓冲区使用标志
	 * @param memoryUsage 内存使用标志
	 * @return AllocatedBuffer 缓冲区
	 */
	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	MaterialInstance create_metallic_roughness_instance(MaterialPass pass, const GLTFMetallicRoughness::MaterialResources& materialResources, const GLTFMetallicRoughness::MaterialConstants& materialConstants, DescriptorAllocatorGrowable& descriptorAllocator)
	{
		return _metalRoughnessMaterial.create_material_instance(_device, pass, materialResources, materialConstants, descriptorAllocator);
	}

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
	 * @brief 初始化网格管线
	 */
	void init_mesh_pipeline();
	
	/**
	 * @brief 初始化默认数据
	 */
	void init_default_data();

	/**
	 * @brief 初始化imgui
	 */
	void init_imgui();

	/**
	 * @brief 创建图像，只分配内存
	 * @param extent 图像大小
	 * @param format 图像格式
	 * @param usage 图像使用标志
	 * @param mipmapped 是否启用mipmap
	 * @return AllocatedImage 图像
	 */
	AllocatedImage create_image(VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

	/**
	 * @brief 创建图像，并拷贝数据到图像中
	 * @param data 图像数据
	 * @param extent 图像大小
	 * @param format 图像格式
	 * @param usage 图像使用标志
	 * @param mipmapped 是否启用mipmap
	 * @return AllocatedImage 图像
	 */
	AllocatedImage create_image(void* data, VkExtent3D extent, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	
	/**
	 * @brief 销毁缓冲区
	 * @param buffer 缓冲区
	 */
	void destroy_buffer(const AllocatedBuffer& buffer);

	/**
	 * @brief 销毁图像
	 * @param image 图像
	 */
	void destroy_image(const AllocatedImage& image);

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
	
	/**
	 * @brief 重新创建交换链
	 */
	void resize_swapchain();

	/**
	 * @brief 更新场景
	 */
	void update_scene();

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
	// 分配深度图像
	AllocatedImage _depthImage;

	// 描述符分配器
	DescriptorAllocatorGrowable _globalDescriptorAllocator;

	// 描述符集
	VkDescriptorSet _drawImageDescriptors;
	// 描述符集布局
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	// 场景数据
	GPUSceneData _sceneData;
	// 场景数据描述符集布局
	VkDescriptorSetLayout _sceneDataDescriptorLayout;

	// imgui 相关
	// 栅栏	
	VkFence _immFence;
	// 命令池
	VkCommandPool _immCmdPool;
	// 命令缓冲区
	VkCommandBuffer _immCmdBuffer;
	
	// 是否需要重新创建交换链
	bool _resizeRequested{ false };
	// 渲染缩放
	float _renderScale {1.f};
	
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
	
	// 网格管线
	VkPipeline _meshPipeline;
	// 网格管线布局
	VkPipelineLayout _meshPipelineLayout;
	// 网格数据
	GPUMeshBuffers _meshBuffers;
	// 几何体集合
	std::vector<std::shared_ptr<MeshAsset>> _testMeshes;

	// 白色图像
	AllocatedImage _whiteImage;
	// 黑色图像
	AllocatedImage _blackImage;
	// 灰色图像
	AllocatedImage _greyImage;
	// 错误棋盘图像
	AllocatedImage _errorCheckerboardImage;

	// 默认线性采样器
	VkSampler _defaultSamplerLinear;
	// 默认最近邻采样器
	VkSampler _defaultSamplerNearest;

	// 单个图像描述符集布局	
	VkDescriptorSetLayout _singleImageDescriptorLayout;

	// 材质
	// 具有金属度和粗糙度的材质
	GLTFMetallicRoughness _metalRoughnessMaterial;
	// 默认材质实例，用于绘制物体
	MaterialInstance _defaultInstance;

	// 渲染上下文
	DrawContext _drawContext;
	// 加载的节点
	std::unordered_map<std::string, std::shared_ptr<Node>> _loadedNodes;

	// 相机
	Camera _mainCamera;
	// 上次更新时间
	std::chrono::steady_clock::time_point _lastTime;

	// 加载的gltf模型
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> _loadedGLTFs;
};
