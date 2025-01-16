#pragma once

#include "vk_types.h"
#include "vk_loader.h"
#include "vk_materials.h"

struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    std::shared_ptr<MaterialInstance> material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
    std::vector<RenderObject> opaqueSurfaces;
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

    virtual void draw(const glm::mat4& topMatrix, DrawContext& drawContext) override;
};