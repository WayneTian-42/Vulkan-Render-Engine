#pragma once

#include "vk_types.h"

#include <unordered_map>
#include <filesystem>

// 几何体表面
struct GeoSurface {
    uint32_t startIndex;
    uint32_t indexCount;
};

// 几何体
struct MeshAsset {
    std::string name;
    
    // 几何体表面
    std::vector<GeoSurface> surfaces;
    // 几何体缓冲区
    GPUMeshBuffers meshBuffers;
};

// 前向声明
class VulkanEngine;

/**
 * @brief 加载GLTF文件
 * @param engine 引擎
 * @param path 文件路径
 * @return 几何体集合或者空
 */
std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_files(VulkanEngine* engine, const std::filesystem::path& path);
