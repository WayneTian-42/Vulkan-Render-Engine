#pragma once

#include <vk_types.h>

// 描述符布局构建器
struct DescriptorLayoutBuilder {
    // 描述符集布局绑定列表
	std::vector<VkDescriptorSetLayoutBinding> bindings;

    /**
     * @brief 添加一个绑定
     * @param binding 绑定的索引
     * @param type 描述符类型
     */
    void add_binding(uint32_t binding, VkDescriptorType type);

    /**
     * @brief 清空绑定列表
     */
    void clear();

    /**
     * @brief 构建描述符集布局
     * @param device 逻辑设备
     * @param shaderStages 着色器阶段
     * @param pNext 扩展结构体指针
     * @param flags 创建标志
     * @return 描述符集布局
     */
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator {

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    // std::span是C++20引入的，用于表示一个连续的内存区域，与std::vector不同之处在于它不拥有数据，数据由其他容器管理
    /**
     * @brief 初始化描述符池
     * @param device 逻辑设备
     * @param maxSets 最大描述符集数量
     */
    void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);

    /**
     * @brief 清除描述符池
     */
    void clear_descriptors(VkDevice device);

    /**
     * @brief 销毁描述符池
     */
    void destroy_pool(VkDevice device);

    /**
     * @brief 分配描述符集
     * @param device 逻辑设备
     * @param layout 描述符集布局
     * @return 描述符集
     */
    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable {
public:

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    /**
     * @brief 初始化描述符池
     * @param device 逻辑设备
     * @param initalSets 初始描述符集数量
     * @param poolRatios 池大小比例
     */
    void init(VkDevice device, uint32_t initalSets, std::span<PoolSizeRatio> poolRatios);

    /**
     * @brief 清除描述符池
     * @param device 逻辑设备
     */
    void clear_pools(VkDevice device);

    /**
     * @brief 销毁描述符池
     * @param device 逻辑设备
     */
    void destroy_pool(VkDevice device);

    /**
     * @brief 分配描述符集
     * @param device 逻辑设备
     * @param layout 描述符集布局
     * @param pNext 扩展结构体指针
     * @return 描述符集
     */
    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:

    /**
     * @brief 获取池
     * @param device 逻辑设备
     * @return 描述符池
     */
    VkDescriptorPool get_pool(VkDevice device);

    /**
     * @brief 创建池
     * @param device 逻辑设备
     * @param setCount 描述符集数量
     * @param poolRatios 池大小比例
     * @return 描述符池
     */
    VkDescriptorPool create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);
    
private:
    // 池大小比例   
    std::vector<PoolSizeRatio> _poolRatios;
    // 已满的池
    std::vector<VkDescriptorPool> _fullPools;
    // 可用的池
    std::vector<VkDescriptorPool> _readyPools;

    // 每个池的描述符集数量
    uint32_t _setsPerPool;
};

struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    /**
     * @brief 写入图像描述符
     * @param binding 绑定索引
     * @param imageView 图像视图
     * @param sampler 采样器
     * @param imageLayout 图像布局
     * @param type 描述符类型
     */
    void write_image(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout, VkDescriptorType type);

    /**
     * @brief 写入缓冲区描述符
     * @param binding 绑定索引
     * @param buffer 缓冲区
     * @param offset 偏移量
     * @param range 范围
     * @param type 描述符类型
     */
    void write_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, VkDescriptorType type);

    /**
     * @brief 清除描述符
     */
    void clear();

    /**
     * @brief 更新描述符集
     * @param device 逻辑设备
     * @param set 描述符集
     */
    void update_set(VkDevice device, VkDescriptorSet set);
};