#pragma once

#include "vk_materials.h"
#include <memory>

// 材质管理器类，使用单例模式
class MaterialManager {
public:
    // 获取单例实例
    static MaterialManager& Get();

    // 删除拷贝构造函数和赋值运算符
    MaterialManager(const MaterialManager&) = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;

    // 初始化材质系统
    void init_material_systems();

    // 清理资源
    void cleanup();

    // 获取金属粗糙度材质
    std::shared_ptr<GLTFMetallicRoughness> get_metallic_roughness_material() const { 
        return _metallicRoughnessMaterial; 
    }

    // 获取PBR材质
    std::shared_ptr<PBRMaterial> get_pbr_material() const { 
        return _pbrMaterial; 
    }

private:
    // 私有构造函数
    MaterialManager() = default;

    // 材质实例
    std::shared_ptr<GLTFMetallicRoughness> _metallicRoughnessMaterial;
    std::shared_ptr<PBRMaterial> _pbrMaterial;
}; 