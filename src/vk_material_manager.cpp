#include "vk_material_manager.h"
#include "vk_engine.h"

MaterialManager& MaterialManager::Get() {
    static MaterialManager instance;
    return instance;
}

void MaterialManager::init_material_systems() {
    // 初始化金属粗糙度材质
    _metallicRoughnessMaterial = std::make_shared<GLTFMetallicRoughness>();
    _metallicRoughnessMaterial->build_pipelines();

    // 初始化PBR材质
    _pbrMaterial = std::make_shared<PBRMaterial>();
    _pbrMaterial->build_pipelines();
}

void MaterialManager::cleanup() {
    // 清理金属粗糙度材质
    if (_metallicRoughnessMaterial) {
        _metallicRoughnessMaterial->clear_resources(VulkanEngine::Get().get_device());
        _metallicRoughnessMaterial.reset();
    }

    // 清理PBR材质
    if (_pbrMaterial) {
        _pbrMaterial->clear_resources(VulkanEngine::Get().get_device());
        _pbrMaterial.reset();
    }
} 