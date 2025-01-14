#include "vk_loader.h"

#include <stb_image.h>
#include <iostream>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

// filesystem::path是 C++17 引入的文件系统库中的一个类，用于表示和操作文件路径。
// 它提供了一种跨平台的方式来处理文件路径，能够自动处理不同操作系统之间的路径分隔符差异
std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_files(VulkanEngine* engine, const std::filesystem::path& path) 
{
    std::cout << "Loading GLTF: " << path << std::endl;

    // 加载GLTF文件，创建GLTF数据缓冲区
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);
    
    // 设置加载选项，包括加载GLTF文件和外部缓冲区
    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers;

    // 创建GLTF资产对象
    fastgltf::Asset asset;
    // 创建GLTF解析器
    fastgltf::Parser parser {};
    
    // 加载GLTF文件，以二进制格式加载
    // 传入父级路径，为了引用外部资源，纹理，材质等
    auto load = parser.
        loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
    
    // 如果加载成功，将GLTF资产对象移动到asset
    if (load) {
        asset = std::move(load.get());
    } else {
        fmt::println("Failed to load GLTF file: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }
    
    // 创建一个空的MeshAsset集合
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    
    // 创建一个空的索引集合
    std::vector<uint32_t> indices;
    // 创建一个空的顶点集合
    std::vector<Vertex> vertices;

    // 遍历GLTF资产中的所有几何体
    for (auto& mesh : asset.meshes) {
        MeshAsset newMesh;
        newMesh.name = mesh.name;
        
        // 清空索引和顶点集合
        indices.clear();
        vertices.clear();
        
        // 遍历几何体的所有表面
        for (auto& p : mesh.primitives) {
            GeoSurface surface;
            // 设置表面起始索引
            surface.startIndex = static_cast<uint32_t>(indices.size());
            // 根据索引访问器获取索引数量
            // p.indicesAccessor.value() 是索引访问器的索引
            // asset.accessors是访问器集合
            surface.indexCount = static_cast<uint32_t>(asset.accessors[p.indicesAccessor.value()].count);
            
            // 获取初始顶点数量
            size_t initial_vtx = vertices.size();

            /// 加载索引
            {
                auto& indexAccessor = asset.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(asset, indexAccessor, [&](std::uint32_t index) {
                    indices.push_back(index);
                });
            }

            /// 加载顶点
            {
                auto& positionAccessor = asset.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + positionAccessor.count);
                
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor, 
                    [&](glm::vec3 position, size_t index) {
                        Vertex newVertex;
                        newVertex.position = position;
                        newVertex.normal = glm::vec3(1.f, 0.f, 0.f);
                        newVertex.color = glm::vec4(1.f);
                        newVertex.uv_x = 0.f;
                        newVertex.uv_y = 0.f;
                        
                        vertices[initial_vtx + index] = newVertex;
                    });
            }
            
            // 加载顶点法线
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normals->second], 
                    [&](glm::vec3 normal, size_t index) {
                        vertices[initial_vtx + index].normal = normal;
                    });
            }

            // 加载顶点颜色
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[colors->second], 
                    [&](glm::vec4 color, size_t index) {
                        vertices[initial_vtx + index].color = color;
                    });
            }

            // 加载顶点UV
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uv->second], 
                    [&](glm::vec2 uv, size_t index) {
                        vertices[initial_vtx + index].uv_x = uv.x;
                        vertices[initial_vtx + index].uv_y = uv.y;
                    });
            }
            // 将表面添加到几何体表面集合
            newMesh.surfaces.push_back(surface);
        }
        // 显示顶点法向量
        constexpr bool ShowNormals = true;
        if (ShowNormals) {
            for (auto& vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }

        // 将几何体上传到GPU
        newMesh.meshBuffers = engine->upload_mesh(indices, vertices);

        // 将几何体添加到几何体集合
        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
    }

    return meshes;
}
