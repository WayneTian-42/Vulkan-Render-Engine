#include "vk_loader.h"

#include <iostream>

#include <vk_mem_alloc.h>

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

/**
 * @brief 根据gltf过滤器提取vk过滤器
 * @param filter gltf过滤器
 * @return vk过滤器
 */
VkFilter extract_filter(fastgltf::Filter filter) {
    switch (filter) {
        // 最近邻过滤
        case fastgltf::Filter::Nearest:
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::NearestMipMapLinear:
            return VK_FILTER_NEAREST;
        // 线性过滤
        case fastgltf::Filter::Linear:
        case fastgltf::Filter::LinearMipMapNearest:
        case fastgltf::Filter::LinearMipMapLinear:
            return VK_FILTER_LINEAR;
        default:
            return VK_FILTER_LINEAR;
    }
}

/**
 * @brief 根据gltf过滤器提取vkmipmap模式
 * @param filter gltf过滤器
 * @return vk mipmap模式
 */
VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter) {
    switch (filter) {
        // 最近邻mipmap模式
        case fastgltf::Filter::NearestMipMapNearest:
        case fastgltf::Filter::LinearMipMapNearest:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        // 线性mipmap模式
        case fastgltf::Filter::NearestMipMapLinear:
        case fastgltf::Filter::LinearMipMapLinear:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

// filesystem::path是 C++17 引入的文件系统库中的一个类，用于表示和操作文件路径。
// 它提供了一种跨平台的方式来处理文件路径，能够自动处理不同操作系统之间的路径分隔符差异
// 加载GLTF文件并返回MeshAsset集合
std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(const std::filesystem::path& path) 
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
        constexpr bool ShowNormals = false;
        if (ShowNormals) {
            for (auto& vtx : vertices) {
                vtx.color = glm::vec4(vtx.normal, 1.f);
            }
        }

        // 将网格数据上传到GPU
        newMesh.meshBuffers = VulkanEngine::Get().upload_mesh(indices, vertices);

        // 将网格添加到返回集合中
        meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newMesh)));
    }

    // 返回加载的网格集合
    return meshes;
}

// string_view 是 C++17 引入的用于表示字符串视图的类，它是一个轻量级的、不可变的字符串表示
// 它通常用于传递字符串参数，而不需要拥有实际的字符串数据
std::optional<std::shared_ptr<LoadedGLTF>> load_gltf_files(std::string_view filePath) 
{
    // 打印正在加载的GLTF文件路径
    fmt::println("Loading GLTF: {}", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    // scene->engine = &(VulkanEngine::Get());
    auto& gltf = *(scene.get());

    // 创建GLTF数据缓冲区并加载文件内容
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);
    
    // 设置GLTF加载选项：加载GLB缓冲区和外部缓冲区
    constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::AllowDouble
        | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::DontRequireValidAssetMember;

    // 创建GLTF资产对象和解析器
    fastgltf::Asset asset;
    fastgltf::Parser parser {};

    std::filesystem::path path {filePath};
    
    // 确定GLTF文件类型
    auto type = fastgltf::determineGltfFileType(&data);
    // glTF文本类型
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGLTF(&data,
            path.parent_path(), gltfOptions);
        if (load) {
            asset = std::move(load.get());
        } else {
            std::cerr << "Failed to load GLTF file: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    // GLB二进制类型
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadBinaryGLTF(&data,
            path.parent_path(), gltfOptions);
        if (load) {
            asset = std::move(load.get());
        } else {
            std::cerr << "Failed to load GLTF file: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    } else {
        std::cerr << "Unsupported GLTF file type: " << static_cast<int>(type) << std::endl;
        return {};
    }

    // 分配描述符
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> poolSizeRatios {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
    };

    gltf.descriptorAllocator.init(VulkanEngine::Get().get_device(), asset.materials.size(), poolSizeRatios);

    // 加载samplers
    for (auto& sampler : asset.samplers) {
        VkSamplerCreateInfo samplerInfo {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
        };
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0.0f;

        // 根据gltf过滤器提取vk过滤器
        // magFilter：放大过滤器
        samplerInfo.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        // minFilter：缩小过滤器
        samplerInfo.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        // 根据gltfmipmap模式提取vk mipmap模式
        samplerInfo.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(VulkanEngine::Get().get_device(), &samplerInfo, nullptr, &newSampler);
        // 将新创建的采样器添加到samplers列表中
        gltf.samplers.push_back(newSampler);
    }
    
    // 创建临时存储资产的容器
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    // std::vector<std::shared_ptr<AllocatedImage>> images;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;

    // 加载图像，用于纹理绘制
    for (auto& image : asset.images) {
        auto img = load_gltf_image(asset, image);

        if (img.has_value()) {
            // images.push_back(std::make_shared<AllocatedImage>(img.value()));
            // gltf.images[static_cast<std::string>(image.name)] = std::make_shared<AllocatedImage>(img.value());
            images.push_back(img.value());
            gltf.images[static_cast<std::string>(image.name)] = img.value();
        } else {
            // 暂时采用错误棋盘图像当作默认图像
            // images.push_back(std::make_shared<AllocatedImage>(VulkanEngine::Get().get_error_image()));
            images.push_back(VulkanEngine::Get().get_error_image());
            // 输出错误信息
            fmt::println("Failed to load image: {}", image.name);
            // std::cout << "Failed to load image: " << image.name << std::endl;
        }
    }

    // 创建buffer存储材质数据
    gltf.sceneUniformBuffer = VulkanEngine::Get().create_buffer(sizeof(GLTFMetallicRoughness::MaterialConstants) * asset.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    auto* sceneConstants = static_cast<GLTFMetallicRoughness::MaterialConstants*>
        (gltf.sceneUniformBuffer.allocationInfo.pMappedData);

    int dataIndex = 0;
    // 加载材质信息
    for (auto& material : asset.materials) {
        // 创建材质
        auto newMaterial = std::make_shared<GLTFMaterial>();
        materials.push_back(newMaterial);
        // 存储材质
        gltf.materials[static_cast<std::string>(material.name)] = newMaterial;

        // 设置材质常量
        GLTFMetallicRoughness::MaterialConstants materialConstants;
        materialConstants.colorFactors = glm::make_vec4(material.pbrData.baseColorFactor.data());
        materialConstants.metalRoughFactors.x = material.pbrData.metallicFactor;
        materialConstants.metalRoughFactors.y = material.pbrData.roughnessFactor;
        // 将材质常量存储到buffer中
        sceneConstants[dataIndex] = materialConstants;

        // 根据材质的alpha模式设置材质通道
        MaterialPass passType = MaterialPass::MainColor;
        if (material.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::Transparent;
        }

        // 创建材质资源
        GLTFMetallicRoughness::MaterialResources materialResources;
        // 设置默认纹理
        materialResources.colorImage = VulkanEngine::Get().get_white_image();
        materialResources.colorSampler = VulkanEngine::Get().get_sampler_linear();
        materialResources.metallicRoughnessImage = VulkanEngine::Get().get_white_image();
        materialResources.metallicRoughnessSampler = VulkanEngine::Get().get_sampler_linear();

        // 设置uniform buffer
        materialResources.materialBuffer = gltf.sceneUniformBuffer.buffer;
        materialResources.materialOffset = dataIndex * sizeof(GLTFMetallicRoughness::MaterialConstants);

        // 从gltf文件中获取纹理
        if (material.pbrData.baseColorTexture.has_value()) {
            size_t imageIndex = asset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t samplerIndex = asset.textures[material.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            // materialResources.colorImage = *(images[imageIndex]);
            materialResources.colorImage = images[imageIndex];
            materialResources.colorSampler = gltf.samplers[samplerIndex];
        }
        // 创建材质实例
        newMaterial->instance = VulkanEngine::Get().create_metallic_roughness_instance(passType, materialResources, materialConstants, gltf.descriptorAllocator);

        dataIndex++;
    }
    
    // 创建临时存储索引和顶点的容器
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // 遍历GLTF文件中的所有网格
    for (auto& mesh : asset.meshes) {
        // 创建新的网格资产并设置名称
        std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
        gltf.meshes[static_cast<std::string>(mesh.name)] = newMesh;
        newMesh->name = static_cast<std::string>(mesh.name);
        meshes.push_back(newMesh);
        
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
                fastgltf::iterateAccessor<std::uint32_t>(asset, indexAccessor, 
                    [&](std::uint32_t index) {
                        indices.push_back(index + initial_vtx);
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

            // 设置表面材质，如果材质索引存在，则使用材质索引，否则使用默认材质
            if (p.materialIndex.has_value()) {
                surface.material = materials[p.materialIndex.value()];
            } else {
                surface.material = materials[0];
            }
            // 将表面添加到网格的表面集合中
            newMesh->surfaces.push_back(surface);
        }
        // 将网格数据上传到GPU
        newMesh->meshBuffers = VulkanEngine::Get().upload_mesh(indices, vertices);
    }

    // 加载节点
    for (auto& node : asset.nodes) {
        std::shared_ptr<Node> newNode;

        // 如果节点有网格索引，则创建MeshNode，否则创建Node
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            dynamic_cast<MeshNode*>(newNode.get())->mesh = meshes[*(node.meshIndex)];
        } else {
            newNode = std::make_shared<Node>();
        }

        // 将新节点添加到节点集合中
        nodes.push_back(newNode);
        // 只存储节点名称
        gltf.nodes[static_cast<std::string>(node.name)] = newNode;

        // std::visit是C++17引入的用于访问和处理不同类型数据成员的工具，可以根据不同的类型执行不同的操作
        // 需要定义两个参数，第一个是访问器，即根据不同的类型执行不同的操作，第二个是需要访问的变量，该变量是一个variant类型，该类型可以存储多种不同类型的数据

        // 这里需要访问的是node的transform成员，该成员是一个variant类型，可以存储TransformMatrix和TRS两种类型
        // 定义的访问器根据不同的类型执行不同的操作，如果类型是TransformMatrix，则将matrix复制到newNode的localTransform成员中
        // 如果类型是TRS，则将translation、rotation和scale转换为glm::mat4，并计算出transformMatrix
        std::visit(
            fastgltf::visitor {
                // 如果类型是TransformMatrix，则将matrix复制到newNode的localTransform成员中
                [newNode](fastgltf::Node::TransformMatrix matrix) {
                    memcpy(glm::value_ptr(newNode->localTransform), matrix.data(), sizeof(matrix));
                },
                // 如果类型是TRS，则将translation、rotation和scale转换为glm::mat4，并计算出transformMatrix
                // TRS是Transform、Rotation、Scale的缩写，分别表示平移、旋转和缩放
                [newNode](fastgltf::Node::TRS transform) {
                    glm::vec3 tl = glm::make_vec3(transform.translation.data());
                    glm::quat rot = glm::make_quat(transform.rotation.data());
                    glm::vec3 sc = glm::make_vec3(transform.scale.data());

                    glm::mat4 transformMatrix = glm::translate(glm::mat4(1.f), tl);
                    glm::mat4 rotMatrix = glm::mat4_cast(rot);
                    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), sc);

                    // 将平移、旋转和缩放矩阵相乘，得到最终的变换矩阵，注意矩阵的乘法顺序，先缩放，再旋转，最后平移
                    newNode->localTransform = transformMatrix * rotMatrix * scaleMatrix;
                }
            }, 
            node.transform);
    }

    // 遍历节点，设置节点的层级关系
    for (size_t i = 0; i < nodes.size(); i++) {
        auto node = asset.nodes[i];
        auto& sceneNode = nodes[i];

        // 遍历节点的子节点
        for (auto& child : node.children) {
            // 将子节点添加到场景节点的子节点集合中
            sceneNode->children.push_back(nodes[child]);
            // 将场景节点的索引设置为子节点的父节点索引
            nodes[child]->parent = sceneNode;
        }
    }

    // 找到所有的根节点
    for (auto& node : nodes) {
        // 如果节点的父节点是空的，则将该节点添加到根节点集合中
        // expired()是std::shared_ptr的成员函数，用于检查指针是否为空
        if (node->parent.expired()) {
            // 将根节点添加到gltf的根节点集合中
            gltf.rootNodes.push_back(node);
            // 更新根节点下的所有节点
            node->refreshTransform(glm::mat4(1.f));
        }
    }

    // 返回加载的GLTF场景
    return scene;
}

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
        renderObject.material = std::make_shared<MaterialInstance>(surface.material->instance);

        renderObject.transform = nodeMatrix;
        renderObject.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

        drawContext.opaqueSurfaces.push_back(renderObject);
    }
    Node::draw(nodeMatrix, drawContext);
}

void LoadedGLTF::draw(const glm::mat4& topMatrix, DrawContext& drawContext)
{
    // 遍历根节点，绘制场景
    for (auto& node : rootNodes) {
        node->draw(topMatrix, drawContext);
    }
}

void LoadedGLTF::clear_all()
{
    VkDevice device = VulkanEngine::Get().get_device();

    descriptorAllocator.destroy_pool(device);
    VulkanEngine::Get().destroy_buffer(sceneUniformBuffer);

    for (auto& [k, v] : meshes) {
        VulkanEngine::Get().destroy_buffer(v->meshBuffers.indexBuffer);
        VulkanEngine::Get().destroy_buffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images) {
        if (v.image == VulkanEngine::Get().get_error_image().image) {
            continue;
        }
        VulkanEngine::Get().destroy_image(v);
    }

    for (auto& sampler : samplers) {
        vkDestroySampler(device, sampler, nullptr);
    }
}

LoadedGLTF::~LoadedGLTF()
{
    clear_all();
}

std::optional<AllocatedImage> load_gltf_image(fastgltf::Asset& asset, fastgltf::Image& image)
{
    AllocatedImage newImage {};
    
    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            // 默认情况，当数据是其他类型时，不做任何处理
            [](auto& arg) {},

            // 如果数据是URI类型，则从文件中加载图像数据
            [&](fastgltf::sources::URI& filePath) {
                // 断言，确保文件偏移量为0
                assert(filePath.fileByteOffset == 0);
                // 断言，确保文件路径是本地路径
                assert(filePath.uri.isLocalPath());

                // 基于string_view创建string，这种方法可以避免拷贝string
                const std::string path{filePath.uri.path().begin(), filePath.uri.path().end()};

                // 加载图像数据
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);

                // 如果加载成功，则创建图像
                if (data) {
                    VkExtent3D imageExtent;
                    imageExtent.width = static_cast<uint32_t>(width);
                    imageExtent.height = static_cast<uint32_t>(height);
                    imageExtent.depth = 1;

                    // 创建图像
                    newImage = VulkanEngine::Get().create_image(data, imageExtent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
                    // 释放图像数据
                    stbi_image_free(data);
                }
            },

            // 如果数据是vector类型，则从内存中加载图像数据
            [&](fastgltf::sources::Vector& vector) {
                // 加载图像数据
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, 
                    &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imageExtent;
                    imageExtent.width = static_cast<uint32_t>(width);
                    imageExtent.height = static_cast<uint32_t>(height);
                    imageExtent.depth = 1;

                    // 创建图像
                    newImage = VulkanEngine::Get().create_image(data, imageExtent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
                    // 释放图像数据
                    stbi_image_free(data);
                }
            },

            // 如果数据是buffer类型，则从缓冲区中加载图像数据
            [&](fastgltf::sources::BufferView& view) {
                // 获取缓冲区视图
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                // 获取缓冲区
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(
                    fastgltf::visitor {
                        [](auto& arg) {},

                        // 如果数据是vector类型，则从内存中加载图像数据，这里只考虑VectorWithMime
                        // 因为在加载数据时指定了LoadExternalBuffer，所以所有数据都已经存储在vector中
                        [&](fastgltf::sources::Vector& vector) {
                            // 加载图像数据
                            // 数据从bufferView.byteOffset开始，长度为bufferView.byteLength
                            unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
                            // 如果加载成功，则创建图像
                            if (data) {
                                VkExtent3D imageExtent;
                                imageExtent.width = static_cast<uint32_t>(width);
                                imageExtent.height = static_cast<uint32_t>(height);
                                imageExtent.depth = 1;

                                // 创建图像
                                newImage = VulkanEngine::Get().create_image(data, imageExtent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);
                                // 释放图像数据
                                stbi_image_free(data);
                            }
                        }
                    }, 
                    buffer.data);
            }
        },
        image.data
    );

    // 如果图像创建失败，则返回空
    if (newImage.image == VK_NULL_HANDLE) {
        return std::nullopt;
    }

    return newImage;
}