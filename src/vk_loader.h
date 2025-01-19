#pragma once

#include "vk_types.h"
#include "vk_materials.h"

#include <fastgltf/types.hpp>
#include <unordered_map>
#include <filesystem>

struct GLTFMaterial {
    MaterialInstance instance;
    // 添加材质类型枚举
    enum class Type {
        MetallicRoughness,
        PBR
    } type;
};

// 几何体的边界信息
struct Bounds {
    glm::vec3 origin;      // 边界框的中心点
    float sphereRadius;     // 包围球半径
    glm::vec3 extents;     // 边界框的尺寸(长宽高的一半)，表示包围盒在每个轴上的长度
};

// 几何体表面结构体，用于存储几何体表面的相关信息
// 一个surface可以认为是一个三角形
struct GeoSurface {
    uint32_t startIndex;   // 索引缓冲区中的起始索引
    uint32_t indexCount;   // 索引数量
    Bounds bounds;         // 几何体的边界信息
    std::shared_ptr<GLTFMaterial> material;  // 表面材质
};

// 几何体，一个几何体可以包含多个表面
struct MeshAsset {
    std::string name;
    
    // 几何体表面
    std::vector<GeoSurface> surfaces;
    // 几何体缓冲区
    GPUMeshBuffers meshBuffers;
};

// 渲染对象，包含了渲染一个物体所需的所有信息
// 包括几何体自身的属性信息、材质信息以及渲染所需的缓冲区地址
struct RenderObject {
    uint32_t indexCount;           // 索引数量
    uint32_t firstIndex;          // 起始索引
    VkBuffer indexBuffer;         // 索引缓冲区

    // 材质实例，使用智能指针管理
    std::shared_ptr<MaterialInstance> material;

    // 物体的边界信息
    Bounds bounds;

    // 变换矩阵
    glm::mat4 transform;
    // 顶点缓冲区的设备地址
    VkDeviceAddress vertexBufferAddress;
};

// 绘制上下文，用于存储需要渲染的物体
struct DrawContext {
    std::vector<RenderObject> opaqueSurfaces;      // 不透明物体的集合
    std::vector<RenderObject> transparentSurfaces;  // 透明物体的集合
};

// 接口类，可绘制对象
class IRenderable {

    /**
     * @brief 绘制
     * @param topMatrix 顶层矩阵
     * @param drawContext 绘制上下文
     */
    virtual void draw(const glm::mat4& topMatrix, DrawContext& drawContext) = 0;
};

// 一个可绘制的场景节点，该节点可以具有子节点，并且可以将变换矩阵传递给子节点
struct Node : public IRenderable {

    // 父节点，使用弱引用，避免循环引用
    std::weak_ptr<Node> parent;
    // 子节点
    std::vector<std::shared_ptr<Node>> children;

    // 变换矩阵
    glm::mat4 localTransform;
    // 世界变换矩阵
    glm::mat4 worldTransform;

    /**
     * @brief 刷新世界变换矩阵
     * @param parentMatrix 父节点变换矩阵
     */
    void refreshTransform(const glm::mat4& parentMatrix);

    /**
     * @brief 绘制
     * @param topMatrix 顶层矩阵
     * @param drawContext 绘制上下文
     */
    virtual void draw(const glm::mat4& topMatrix, DrawContext& drawContext) override;
};

struct MeshNode : public Node {
    // 网格
    std::shared_ptr<MeshAsset> mesh;

    /**
     * @brief 绘制
     * @param topMatrix 顶层矩阵
     * @param drawContext 绘制上下文
     */
    virtual void draw(const glm::mat4& topMatrix, DrawContext& drawContext) override;
};

// 前向声明
class VulkanEngine;

struct LoadedGLTF : public IRenderable {
    // 存储网格资源，key为网格名称，value为网格资源
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    // 存储节点，key为节点名称，value为节点
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    // 存储图像资源，key为图像名称，value为图像资源
    // std::unordered_map<std::string, std::shared_ptr<AllocatedImage>> images;
    std::unordered_map<std::string, AllocatedImage> images;
    // 存储材质资源，key为材质名称，value为材质资源
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // 存储根节点，即没有父节点的节点
    std::vector<std::shared_ptr<Node>> rootNodes;

    // 存储采样器
    std::vector<VkSampler> samplers;

    // 描述符分配器，用于分配描述符
    DescriptorAllocatorGrowable descriptorAllocator;

    // 场景统一缓冲区，用于存储场景相关的数据
    AllocatedBuffer sceneUniformBuffer;

    //todo: 将材质管理器抽象成一个类，并使用单例模式
    // 添加材质管理器
    std::shared_ptr<GLTFMetallicRoughness> metallicRoughnessMaterial;
    std::shared_ptr<PBRMaterial> pbrMaterial;

    // todo: 使用shared_ptr或者weak_ptr时出现问题
    // *使用单例模式，规避指针的各种问题
    // 使用shared_ptr时，会出现循环引用，导致无法释放资源
    // 使用weak_ptr时，初始化时程序崩溃
    // VulkanEngine* engine;

public:

    /**
     * @brief 析构函数
     */
    ~LoadedGLTF();

    /**
     * @brief 绘制
     * @param topMatrix 顶层矩阵
     * @param drawContext 绘制上下文
     */
    virtual void draw(const glm::mat4& topMatrix, DrawContext& drawContext) override;

    /**
     * @brief 初始化材质系统
     */
    void init_material_systems();

private:

    /**
     * @brief 清除所有资源
     */
    void clear_all();
};

/**
 * @brief 加载GLTF文件
 * @param engine 引擎
 * @param path 文件路径
 * @return 几何体集合或者空
 */
std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(const std::filesystem::path& path);

/**
 * @brief 加载GLTF文件
 * @param engine 引擎
 * @param path 文件路径
 * @return 加载的gltf模型或者空
 */
std::optional<std::shared_ptr<LoadedGLTF>> load_gltf_files(std::string_view path);

/**
 * @brief 加载GLTF图片
 * @param engine 引擎
 * @param asset gltf模型
 * @param image 图片
 * @return 加载的图片或者空
 */
std::optional<AllocatedImage> load_gltf_image(fastgltf::Asset& asset, fastgltf::Image& image);

// 材质加载器命名空间
namespace material_loader {
    /**
     * @brief 加载PBR材质
     */
    std::shared_ptr<GLTFMaterial> load_pbr_material(
        fastgltf::Material& material,
        LoadedGLTF& gltf,
        void* materialData,
        int dataIndex,
        size_t materialStride,
        const std::vector<AllocatedImage>& images,
        const std::vector<VkSampler>& samplers,
        const std::vector<fastgltf::Texture>& textures
    );

    /**
     * @brief 加载金属粗糙度材质
     */
    std::shared_ptr<GLTFMaterial> load_metallic_roughness_material(
        fastgltf::Material& material,
        LoadedGLTF& gltf,
        void* materialData,
        int dataIndex,
        size_t materialStride,
        const std::vector<AllocatedImage>& images,
        const std::vector<VkSampler>& samplers,
        const std::vector<fastgltf::Texture>& textures
    );

    /**
     * @brief 根据材质类型加载对应的材质
     */
    std::shared_ptr<GLTFMaterial> load_material(
        fastgltf::Material& material,
        LoadedGLTF& gltf,
        void* materialData,
        int dataIndex,
        size_t materialStride,
        const std::vector<AllocatedImage>& images,
        const std::vector<VkSampler>& samplers,
        const std::vector<fastgltf::Texture>& textures
    );
}