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
// 加载GLTF文件并返回MeshAsset集合
std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_files(VulkanEngine* engine, const std::filesystem::path& path) 
{
    // 打印正在加载的GLTF文件路径
    std::cout << "Loading GLTF: " << path << std::endl;

    // 创建GLTF数据缓冲区并加载文件内容
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(path);
    
    // 设置GLTF加载选项：加载GLB缓冲区和外部缓冲区
    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers
        | fastgltf::Options::LoadExternalBuffers;

    // 创建GLTF资产对象和解析器
    fastgltf::Asset asset;
    fastgltf::Parser parser {};
    
    // 以二进制格式加载GLTF文件，父级路径是为了加载纹理、材质等外部信息
    auto load = parser.loadBinaryGLTF(&data,
        path.parent_path(), gltfOptions);
    
    // 检查加载是否成功
    if (load) {
        // 成功则移动加载结果到asset
        asset = std::move(load.get());
    } else {
        // 失败则打印错误信息并返回空
        fmt::println("Failed to load GLTF file: {} \n", fastgltf::to_underlying(load.error()));
        return {};
    }
    
    // 创建存储网格资产的容器
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    
    // 创建临时存储索引和顶点的容器
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // 遍历GLTF文件中的所有网格
    for (auto& mesh : asset.meshes) {
        // 创建新的网格资产并设置名称
        MeshAsset newMesh;
        newMesh.name = mesh.name;
        
        // 清空临时容器
        indices.clear();
        vertices.clear();
        
        // 遍历网格中的所有图元（表面）
        for (auto& p : mesh.primitives) {
            // 创建新的几何表面
            GeoSurface surface;
            // 设置表面起始索引
            surface.startIndex = static_cast<uint32_t>(indices.size());
            // 设置表面索引数量
            surface.indexCount = static_cast<uint32_t>(asset.accessors[p.indicesAccessor.value()].count);
            
            // 记录当前顶点数量
            size_t initial_vtx = vertices.size();

            // 加载索引数据
            {
                auto& indexAccessor = asset.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                // 遍历访问器中的索引数据
                fastgltf::iterateAccessor<std::uint32_t>(asset, indexAccessor, [&](std::uint32_t index) {
                    indices.push_back(index);
                });
            }

            // 加载顶点位置数据
            {
                auto& positionAccessor = asset.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + positionAccessor.count);
                
                // 遍历访问器中的顶点位置数据
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor, 
                    [&](glm::vec3 position, size_t index) {
                        // 创建新顶点并设置默认值
                        Vertex newVertex;
                        newVertex.position = position;
                        newVertex.normal = glm::vec3(1.f, 0.f, 0.f); // 默认法线
                        newVertex.color = glm::vec4(1.f); // 默认颜色
                        newVertex.uv_x = 0.f; // 默认UV
                        newVertex.uv_y = 0.f;
                        
                        // 将顶点存入容器
                        vertices[initial_vtx + index] = newVertex;
                    });
            }
            
            // 加载法线数据（如果存在）
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normals->second], 
                    [&](glm::vec3 normal, size_t index) {
                        vertices[initial_vtx + index].normal = normal;
                    });
            }

            // 加载顶点颜色数据（如果存在）
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[colors->second], 
                    [&](glm::vec4 color, size_t index) {
                        vertices[initial_vtx + index].color = color;
                    });
            }

            // 加载UV坐标数据（如果存在）
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uv->second], 
                    [&](glm::vec2 uv, size_t index) {
                        vertices[initial_vtx + index].uv_x = uv.x;
                        vertices[initial_vtx + index].uv_y = uv.y;
                    });
            }
            // 将表面添加到网格的表面集合中
            newMesh.surfaces.push_back(surface);
        }
        // 调试选项：将法线显示为颜色
        constexpr bool ShowNormals = true;
        if (ShowNormals) {
            for (auto& vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }

        // 将网格数据上传到GPU
        newMesh.meshBuffers = engine->upload_mesh(indices, vertices);

        // 将网格添加到返回集合中
        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
    }

    // 返回加载的网格集合
    return meshes;
}
