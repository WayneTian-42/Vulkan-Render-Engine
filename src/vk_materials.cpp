#include "vk_materials.h"
#include "vk_pipelines.h"
#include "vk_engine.h"

void Material::clear_resources(VkDevice device) {
    vkDestroyDescriptorSetLayout(device, materialSetLayout, nullptr);
    vkDestroyPipelineLayout(device, opaquePipeline.layout, nullptr);

    vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
    vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
}

void Material::create_pipeline_layout(VkDevice device, 
    const VkDescriptorSetLayout* descriptorSetLayouts,
    uint32_t setLayoutCount,
    const VkPushConstantRange& pushConstantRange) {
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .setLayoutCount = setLayoutCount,
        .pSetLayouts = descriptorSetLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    opaquePipeline.layout = pipelineLayout;
    transparentPipeline.layout = pipelineLayout;
}

void GLTFMetallicRoughness::build_pipelines() {
    // 加载着色器
    VkShaderModule meshVertShader;
    if (!vkutil::load_shader_module("../shaders/mesh.vert.spv", VulkanEngine::Get().get_device(), &meshVertShader)) {
        fmt::println("Error when building the mesh vertex shader");
        return;
    }

    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("../shaders/mesh.frag.spv", VulkanEngine::Get().get_device(), &meshFragShader)) {
        fmt::println("Error when building the mesh fragment shader");
        return;
    }

    // 创建材质常量
    VkPushConstantRange materialPushConstantRange {};
    materialPushConstantRange.offset = 0;
    materialPushConstantRange.size = sizeof(MaterialConstants);
    materialPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // 创建材质描述符布局
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialSetLayout = builder.build(VulkanEngine::Get().get_device(), 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {
        VulkanEngine::Get().get_scene_set_layout(), 
        materialSetLayout
    };

    // 使用基类方法创建管线布局
    create_pipeline_layout(VulkanEngine::Get().get_device(), layouts, 2, materialPushConstantRange);

    // 创建管线
    vkutil::PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depth_test(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // 设置颜色和深度附件格式
    pipelineBuilder.set_color_attachment_format(VulkanEngine::Get().get_draw_image_format());
    pipelineBuilder.set_depth_attachment_format(VulkanEngine::Get().get_depth_image_format());

    pipelineBuilder._pipelineLayout = opaquePipeline.layout;

    // 构建不透明管线
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(VulkanEngine::Get().get_device());

    // 构建透明管线
    pipelineBuilder.enable_blending_additive();
    pipelineBuilder.enable_depth_test(VK_FALSE, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(VulkanEngine::Get().get_device());

    // 销毁着色器
    vkDestroyShaderModule(VulkanEngine::Get().get_device(), meshVertShader, nullptr);
    vkDestroyShaderModule(VulkanEngine::Get().get_device(), meshFragShader, nullptr);
}

MaterialInstance GLTFMetallicRoughness::create_material_instance(VkDevice device, MaterialPass pass, const MaterialResources& resources, const MaterialConstants& constants, DescriptorAllocatorGrowable& descriptorAllocator) {
    MaterialInstance instance;
    instance.pass = pass;

    if (pass == MaterialPass::Transparent) {
        instance.pipeline = std::make_shared<MaterialPipeline>(transparentPipeline);
    } else {
        instance.pipeline = std::make_shared<MaterialPipeline>(opaquePipeline);
    }

    // 创建材质描述符集
    instance.materialSet = descriptorAllocator.allocate(device, materialSetLayout);

    // 写入材质描述符
    materialWriter.clear();
    materialWriter.write_buffer(0, resources.materialBuffer, resources.materialOffset, sizeof(MaterialConstants), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    materialWriter.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    materialWriter.write_image(2, resources.metallicRoughnessImage.imageView, resources.metallicRoughnessSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    // 更新材质描述符集
    materialWriter.update_set(device, instance.materialSet);

    return instance;
}

void PBRMaterial::build_pipelines() {
    // 加载PBR着色器
    VkShaderModule pbrVertShader;
    if (!vkutil::load_shader_module("../shaders/basic_pbr.vert.spv", VulkanEngine::Get().get_device(), &pbrVertShader)) {
        fmt::println("Error when building the PBR vertex shader");
        return;
    }

    VkShaderModule pbrFragShader;
    if (!vkutil::load_shader_module("../shaders/basic_pbr.frag.spv", VulkanEngine::Get().get_device(), &pbrFragShader)) {
        fmt::println("Error when building the PBR fragment shader");
        return;
    }

    // 创建材质常量
    VkPushConstantRange materialPushConstantRange{};
    materialPushConstantRange.offset = 0;
    materialPushConstantRange.size = sizeof(MaterialConstants);
    materialPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // 创建材质描述符布局
    DescriptorLayoutBuilder builder;
    // 材质常量缓冲
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    // 基础颜色贴图
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    // 金属度-粗糙度贴图
    builder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    // 法线贴图
    builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    // 自发光贴图
    builder.add_binding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    // 环境光遮蔽贴图
    builder.add_binding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialSetLayout = builder.build(VulkanEngine::Get().get_device(), 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {
        VulkanEngine::Get().get_scene_set_layout(), 
        materialSetLayout
    };

    // 使用基类方法创建管线布局
    create_pipeline_layout(VulkanEngine::Get().get_device(), layouts, 2, materialPushConstantRange);

    // 创建管线
    vkutil::PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(pbrVertShader, pbrFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depth_test(VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // 设置颜色和深度附件格式
    pipelineBuilder.set_color_attachment_format(VulkanEngine::Get().get_draw_image_format());
    pipelineBuilder.set_depth_attachment_format(VulkanEngine::Get().get_depth_image_format());

    pipelineBuilder._pipelineLayout = opaquePipeline.layout;

    // 构建不透明管线
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(VulkanEngine::Get().get_device());

    // 构建透明管线
    pipelineBuilder.enable_blending_additive();
    pipelineBuilder.enable_depth_test(VK_FALSE, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(VulkanEngine::Get().get_device());

    // 销毁着色器
    vkDestroyShaderModule(VulkanEngine::Get().get_device(), pbrVertShader, nullptr);
    vkDestroyShaderModule(VulkanEngine::Get().get_device(), pbrFragShader, nullptr);
}

MaterialInstance PBRMaterial::create_material_instance(VkDevice device, MaterialPass pass, 
    const MaterialResources& resources, const MaterialConstants& constants, 
    DescriptorAllocatorGrowable& descriptorAllocator) {
    
    MaterialInstance instance;
    instance.pass = pass;

    if (pass == MaterialPass::Transparent) {
        instance.pipeline = std::make_shared<MaterialPipeline>(transparentPipeline);
    } else {
        instance.pipeline = std::make_shared<MaterialPipeline>(opaquePipeline);
    }

    // 创建材质描述符集
    instance.materialSet = descriptorAllocator.allocate(device, materialSetLayout);

    // 写入材质描述符
    materialWriter.clear();
    materialWriter.write_buffer(0, resources.materialBuffer, resources.materialOffset, 
        sizeof(MaterialConstants), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    materialWriter.write_image(1, resources.baseColorImage.imageView, 
        resources.baseColorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    materialWriter.write_image(2, resources.metallicRoughnessImage.imageView, 
        resources.metallicRoughnessSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    materialWriter.write_image(3, resources.normalImage.imageView, 
        resources.normalSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    materialWriter.write_image(4, resources.emissiveImage.imageView, 
        resources.emissiveSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    materialWriter.write_image(5, resources.occlusionImage.imageView, 
        resources.occlusionSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    // 更新材质描述符集
    materialWriter.update_set(device, instance.materialSet);

    return instance;
}

