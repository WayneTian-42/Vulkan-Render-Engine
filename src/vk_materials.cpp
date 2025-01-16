#include "vk_materials.h"
#include "vk_pipelines.h"
#include "vk_engine.h"

void GLTFMetallicRoughness::build_pipelines(VulkanEngine* engine) {
    // 加载着色器
    VkShaderModule meshVertShader;
    if (!vkutil::load_shader_module("../shaders/mesh.vert.spv", engine->get_device(), &meshVertShader)) {
        fmt::println("Error when building the mesh vertex shader");
        return;
    }

    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module("../shaders/mesh.frag.spv", engine->get_device(), &meshFragShader)) {
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

    materialSetLayout = builder.build(engine->get_device(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {engine->get_scene_set_layout(), materialSetLayout};

    // 创建管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
    };

    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = layouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &materialPushConstantRange;

    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->get_device(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

    // 设定管线布局
    opaquePipeline.pipelineLayout = pipelineLayout;
    transparentPipeline.pipelineLayout = pipelineLayout;

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
    pipelineBuilder.set_color_attachment_format(engine->get_draw_image_format());
    pipelineBuilder.set_depth_attachment_format(engine->get_depth_image_format());

    pipelineBuilder._pipelineLayout = pipelineLayout;

    // 构建不透明管线
    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->get_device());

    // 构建透明管线
    pipelineBuilder.enable_blending_additive();
    pipelineBuilder.enable_depth_test(VK_FALSE, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->get_device());

    // 销毁着色器
    vkDestroyShaderModule(engine->get_device(), meshVertShader, nullptr);
    vkDestroyShaderModule(engine->get_device(), meshFragShader, nullptr);
}

void GLTFMetallicRoughness::clear_resources(VkDevice device) {

    vkDestroyDescriptorSetLayout(device, materialSetLayout, nullptr);
    vkDestroyPipelineLayout(device, opaquePipeline.pipelineLayout, nullptr);

    vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
    vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
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

