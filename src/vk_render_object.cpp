#include "vk_render_object.h"

void Node::refreshTransform(const glm::mat4& parentMatrix)
{
    worldTransform = parentMatrix * localTransform;
    for (auto& child : children) {
        child->refreshTransform(worldTransform);
    }
}

void Node::draw(const glm::mat4& topMatrix, DrawContext& drawContext)
{
    for (auto& child : children) {
        child->draw(topMatrix, drawContext);
    }
}

void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& drawContext)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& surface : mesh->surfaces) {
        RenderObject renderObject;
        renderObject.indexCount = surface.indexCount;
        renderObject.firstIndex = surface.startIndex;
        renderObject.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        renderObject.material = std::make_shared<MaterialInstance>(surface.material->data);

        renderObject.transform = nodeMatrix;
        renderObject.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

        drawContext.opaqueSurfaces.push_back(renderObject);
    }
    Node::draw(nodeMatrix, drawContext);
}
