#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"

// 材质管线枚举值
enum class MaterialPass : uint8_t {
	MainColor,
	Transparent,
	Other,
};

// 材质管线
struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

// 材质实例
struct MaterialInstance {
	std::shared_ptr<MaterialPipeline> pipeline;
	VkDescriptorSet materialSet;
	MaterialPass pass;
};

class VulkanEngine;

// 金属粗糙度材质
struct GLTFMetallicRoughness {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialSetLayout;

    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metalRoughFactors;
        // 填充，以256字节对齐
        glm::vec4 extra[14];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metallicRoughnessImage;
        VkSampler metallicRoughnessSampler;
        VkBuffer materialBuffer;
        uint32_t materialOffset;
    };

    DescriptorWriter materialWriter;

    /**
     * @brief 构建管线
     * @param engine 引擎
     */
    void build_pipelines(VulkanEngine* engine);

    /**
     * @brief 清除资源
     * @param device 逻辑设备
     */
    void clear_resources(VkDevice device);

    /**
     * @brief 创建材质实例
     * @param device 设备
     * @param pass 材质管线类型
     * @param resources 材质资源
     * @param constants 材质常量
     * @param descriptorAllocator 描述符分配器
     * @return 材质实例
     */
    MaterialInstance create_material_instance(VkDevice device, MaterialPass pass, const MaterialResources& resources, const MaterialConstants& constants, DescriptorAllocatorGrowable& descriptorAllocator);
};