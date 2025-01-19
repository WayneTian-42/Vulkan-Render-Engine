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
	VkPipelineLayout layout;
};

// 材质实例
struct MaterialInstance {
	std::shared_ptr<MaterialPipeline> pipeline;
	VkDescriptorSet materialSet;
	MaterialPass pass;
};

class VulkanEngine;

// 材质基类
class Material {
protected:
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;
	VkDescriptorSetLayout materialSetLayout;
	DescriptorWriter materialWriter;

public:
	virtual ~Material() = default;

	/**
	 * @brief 构建管线
	 */
	virtual void build_pipelines() = 0;

	/**
	 * @brief 清除资源
	 * @param device 逻辑设备
	 */
	virtual void clear_resources(VkDevice device);

protected:
	/**
	 * @brief 创建基础管线布局
	 * @param device 设备
	 * @param descriptorSetLayouts 描述符集布局数组
	 * @param setLayoutCount 描述符集布局数量
	 * @param pushConstantRange Push常量范围
	 */
	void create_pipeline_layout(VkDevice device, 
		const VkDescriptorSetLayout* descriptorSetLayouts,
		uint32_t setLayoutCount,
		const VkPushConstantRange& pushConstantRange);
};

// 金属粗糙度材质
class GLTFMetallicRoughness : public Material {
public:
	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metalRoughFactors;
		// 填充，以256字节对齐
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;           // 基础颜色贴图
		VkSampler colorSampler;             // 基础颜色采样器
		AllocatedImage metallicRoughnessImage; // 金属度-粗糙度贴图
		VkSampler metallicRoughnessSampler;   // 金属度-粗糙度采样器
		VkBuffer materialBuffer;             // 材质常量缓冲
		uint32_t materialOffset;               // 材质常量偏移
	};

	/**
	 * @brief 构建管线
	 */
	void build_pipelines() override;

	/**
	 * @brief 创建材质实例
	 * @param device 设备
	 * @param pass 材质管线类型
	 * @param resources 材质资源
	 * @param constants 材质常量
	 * @param descriptorAllocator 描述符分配器
	 * @return 材质实例
	 */
	MaterialInstance create_material_instance(VkDevice device, MaterialPass pass, 
		const MaterialResources& resources, const MaterialConstants& constants,
		DescriptorAllocatorGrowable& descriptorAllocator);
};

// 高级PBR材质
class PBRMaterial : public Material {
public:
	struct MaterialConstants {
		glm::vec4 baseColorFactor;      // 基础颜色因子
		glm::vec4 emissiveFactor;       // 自发光因子
		glm::vec4 metallicRoughnessFactor;  // x: 金属度, y: 粗糙度
		glm::vec4 normalScale;          // x: 法线强度
		glm::vec4 occlusionStrength;    // x: 环境光遮蔽强度
		// 填充，以256字节对齐
		glm::vec4 extra[11];
	};

	struct MaterialResources {
		AllocatedImage baseColorImage;           // 基础颜色贴图
		VkSampler baseColorSampler;
		AllocatedImage metallicRoughnessImage;   // 金属度-粗糙度贴图
		VkSampler metallicRoughnessSampler;
		AllocatedImage normalImage;              // 法线贴图
		VkSampler normalSampler;
		AllocatedImage emissiveImage;            // 自发光贴图
		VkSampler emissiveSampler;
		AllocatedImage occlusionImage;           // 环境光遮蔽贴图
		VkSampler occlusionSampler;
		VkBuffer materialBuffer;                 // 材质常量缓冲
		uint32_t materialOffset;
	};

	/**
	 * @brief 构建管线
	 */
	void build_pipelines() override;

	/**
	 * @brief 创建材质实例
	 * @param device 设备
	 * @param pass 材质管线类型
	 * @param resources 材质资源
	 * @param constants 材质常量
	 * @param descriptorAllocator 描述符分配器
	 * @return 材质实例
	 */
	MaterialInstance create_material_instance(VkDevice device, MaterialPass pass,
		const MaterialResources& resources, const MaterialConstants& constants,
		DescriptorAllocatorGrowable& descriptorAllocator);
};