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
