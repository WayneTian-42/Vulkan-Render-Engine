//> includes
#include "vk_engine.h"
#include "vk_images.h"
#include "vk_pipelines.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <glm/gtc/type_ptr.hpp>

// 全局变量，用于单例模式，不使用普通的单例模式是为了能够在创建或者销毁单例时显式地修改指针
VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    init_vulkan();

    init_swapchain();

    init_commands();

    init_sync_structures();

    init_descriptors();

    init_pipelines();
    
    init_imgui();
    
    init_default_data();

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::init_vulkan()
{
    vkb::InstanceBuilder builder;

    // 构造instance，设置app名称，启用验证层，使用默认的debug messenger，要求API版本为1.3.0
    auto inst_builder = builder.set_app_name("Vulkan Engine")
        .request_validation_layers(true)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();
    
    // 获取instance
    vkb::Instance vkb_instance = inst_builder.value();

    // 赋值指定instance和debug messenger
    _instance = vkb_instance.instance;
    _debugMessenger = vkb_instance.debug_messenger;

    // 获取SDL窗口
    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    // vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };
    // 启用动态渲染，用于在渲染过程中动态调整渲染目标
    features.dynamicRendering = VK_TRUE;
    // 启用同步2，用于在渲染过程中动态调整同步
    features.synchronization2 = VK_TRUE;

    // vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };
    // 启用缓冲区设备地址，用于在渲染过程中动态调整缓冲区
    features12.bufferDeviceAddress = VK_TRUE;
    // 启用描述符索引，用于在渲染过程中动态调整描述符
    features12.descriptorIndexing = VK_TRUE;

    // 选择物理设备，使用vkb::InstanceBuilder的select_gpu方法，设置features和features12，并设置surface
    vkb::PhysicalDeviceSelector selector {vkb_instance};

    vkb::PhysicalDevice physical_device = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features)
        .set_required_features_12(features12)
        .set_surface(_surface)
        .select()
        .value();

    // 获取物理设备
    _chosenGPU = physical_device.physical_device;

    // 创建逻辑设备
    vkb::DeviceBuilder device_builder {physical_device};

    vkb::Device vkbDevice = device_builder.build().value();

    // 赋值指定逻辑设备
    _device = vkbDevice.device;

    // 获取图形队列
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    // 获取队列索引
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // 创建内存分配器
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    // 启用缓冲区设备地址
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    // 添加内存分配器销毁函数到删除队列
    _mainDeletionQueue.push_function([this]() {
        vmaDestroyAllocator(_allocator);
    });
}

void VulkanEngine::init_swapchain()
{
    create_swapchain(_windowExtent.width, _windowExtent.height);

    // 创建图像
    VkExtent3D imageExtent = {
        .width = _windowExtent.width,
        .height = _windowExtent.height,
        .depth = 1,
    };

    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = imageExtent;

    VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    VkImageCreateInfo drawImageInfo = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, imageExtent);
    
    // 分配图像内存
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK(vmaCreateImage(_allocator, &drawImageInfo, &allocationInfo,
        &_drawImage.image, &_drawImage.allocation, nullptr));

    // 创建图像视图
    VkImageViewCreateInfo imageViewInfo = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(_device, &imageViewInfo, nullptr, &_drawImage.imageView));

    // 创建深度图像
    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = imageExtent;
    VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    VkImageCreateInfo dimgInfo = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, _depthImage.imageExtent);

    // 分配深度图像
    VK_CHECK(vmaCreateImage(_allocator, &dimgInfo, &allocationInfo,
        &_depthImage.image, &_depthImage.allocation, nullptr));
    
    // 创建深度图像视图
    VkImageViewCreateInfo dimgViewInfo = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(_device, &dimgViewInfo, nullptr, &_depthImage.imageView));


    // 添加图像销毁函数到删除队列
    _mainDeletionQueue.push_function([this]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

        vkDestroyImageView(_device, _depthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
    });
}

void VulkanEngine::init_commands()
{
    // 创建command pool
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        // 分配command buffer
        VkCommandBufferAllocateInfo commandBufferInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &commandBufferInfo, &_frames[i]._commandBuffer));
    }

    // 创建imgui命令池
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCmdPool));

    // 分配imgui命令缓冲区
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCmdPool, 1);

    // 创建imgui命令缓冲区
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCmdBuffer));
    
    // 添加imgui命令缓冲区销毁函数到删除队列
    _mainDeletionQueue.push_function([this]() {
        vkDestroyCommandPool(_device, _immCmdPool, nullptr);
    });
}

void VulkanEngine::init_sync_structures()
{
    // 创建栅栏和信号量
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i]._renderFence));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i]._renderSemaphore));
    }

    // 创建imgui栅栏
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    
    // 添加imgui栅栏销毁函数到删除队列
    _mainDeletionQueue.push_function([this]() {
        vkDestroyFence(_device, _immFence, nullptr);
    });
}

void VulkanEngine::init_descriptors()
{
    // 初始化描述符池的不同类型所占比例
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
    };

    // 创建全局描述符池
    _globalDescriptorAllocator.init_pool(_device, 10, sizes);

    // 创建描述符集布局，将其放在单独的作用域中，以便在创建描述符集后销毁
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // 分配描述符集
    _drawImageDescriptors = _globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

    // 描述符关联的图像信息
    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = _drawImage.imageView;

    // 准备描述符写入操作
    VkWriteDescriptorSet drawImageWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
    };

    drawImageWrite.dstBinding = 0;
    // 目标描述符集，用于指定要更新的描述符集
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    // 描述符关联的图像信息，将图像信息写入描述符集
    drawImageWrite.pImageInfo = &imgInfo;

    // 更新描述符集
    vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

    _mainDeletionQueue.push_function([this]() {
        _globalDescriptorAllocator.destroy_pool(_device);

        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
    });
}

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchain_builder {_chosenGPU, _device, _surface};

    // 设置交换链图像格式
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    VkSurfaceFormatKHR surface_format = {
        .format = _swapchainImageFormat,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    };  

    // 创建交换链
    vkb::Swapchain vkbSwapchain = swapchain_builder
        .set_desired_format(surface_format)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::init_pipelines()
{
    init_background_pipelines();
    init_triangle_pipeline();
    init_mesh_pipeline();
}

void VulkanEngine::init_background_pipelines()
{
    // 创建计算管线布局
    VkPipelineLayoutCreateInfo computeLayout{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
    };
    // 设置描述符集布局
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    // 设置push constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ComputePushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstantRange;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

    // 加载着色器模块
    VkShaderModule gradientColorShader;
    if (!vkutil::load_shader_module("../shaders/gradient_color.comp.spv", _device, &gradientColorShader)) {
        fmt::println("Error when building the compute shader");
    }
    
    VkShaderModule skyShader;
    if (!vkutil::load_shader_module("../shaders/sky.comp.spv", _device, &skyShader)) {
        fmt::println("Error when building the sky shader");
    }

    // 设置着色器阶段信息
    VkPipelineShaderStageCreateInfo shaderStageInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
    };
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = gradientColorShader;
    shaderStageInfo.pName = "main";

    // 设置计算管线创建信息
    VkComputePipelineCreateInfo computePipelineCreateInfo {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
    };
    // 设置管线布局
    computePipelineCreateInfo.layout = _gradientPipelineLayout;
    // 设置着色器阶段
    computePipelineCreateInfo.stage = shaderStageInfo;
    
    // 创建渐变颜色计算管线及配置
    ComputePipeline gradientColorPipeline;
    gradientColorPipeline.name = "gradient_color";
    gradientColorPipeline.pipelineLayout = _gradientPipelineLayout;
    gradientColorPipeline.pushConstants = ComputePushConstants{
        .data1 = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
        .data2 = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f),
    };

    // 创建计算管线
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientColorPipeline.pipeline));
    
    // 创建天空计算管线及配置
    
    computePipelineCreateInfo.stage.module = skyShader;

    ComputePipeline skyPipeline;
    skyPipeline.name = "sky";
    skyPipeline.pipelineLayout = _gradientPipelineLayout;
    skyPipeline.pushConstants = ComputePushConstants{
        .data1 = glm::vec4(0.1f, 0.2f, 0.4f, 0.97f),
    };

    // 创建计算管线
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &skyPipeline.pipeline));
    
    // 将计算管线添加到背景管线列表
    _backgroundPipelines.push_back(gradientColorPipeline);
    _backgroundPipelines.push_back(skyPipeline);

    // 释放着色器模块
    vkDestroyShaderModule(_device, gradientColorShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);

    // 添加销毁函数到删除队列
    _mainDeletionQueue.push_function([gradientColorPipeline, skyPipeline, this]() {
        vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
        vkDestroyPipeline(_device, gradientColorPipeline.pipeline, nullptr);
        vkDestroyPipeline(_device, skyPipeline.pipeline, nullptr);
    });
}

void VulkanEngine::init_triangle_pipeline()
{
    VkShaderModule vertShader, fragShader;
    if (!vkutil::load_shader_module("../shaders/colored_triangle.vert.spv", _device, &vertShader)) {
        fmt::println("Error when building the triangle vertex shader");
    }
    else {
        fmt::println("Triangle Vertex shader loaded successfully");
    }

    if (!vkutil::load_shader_module("../shaders/colored_triangle.frag.spv", _device, &fragShader)) {
        fmt::println("Error when building the triangle fragment shader");
    }
    else {
        fmt::println("Triangle Fragment shader loaded successfully");
    }
    
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_trianglePipelineLayout));

    vkutil::PipelineBuilder builder;
    builder._pipelineLayout = _trianglePipelineLayout;
    // 设置着色器
    builder.set_shaders(vertShader, fragShader);
    // 设置输入拓扑
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // 设置多边形模式
    builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    // 设置剔除模式
    builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // 设置多重采样为无
    builder.set_multisampling_none();
    // 禁用深度和模板测试
    builder.disable_depth_stencil();
    // 禁用颜色混合
    builder.disable_blending();

    // 设置颜色附件格式
    builder.set_color_attachment_format(_drawImage.imageFormat);
    // 设置深度附件格式
    builder.set_depth_attachment_format(VK_FORMAT_UNDEFINED);

    // 构建管线
    _trianglePipeline = builder.build_pipeline(_device);

    // 释放着色器模块
    vkDestroyShaderModule(_device, vertShader, nullptr);
    vkDestroyShaderModule(_device, fragShader, nullptr);
    
    // 添加销毁函数到删除队列
    _mainDeletionQueue.push_function([this]() {
        vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
        vkDestroyPipeline(_device, _trianglePipeline, nullptr);
    });
}

void VulkanEngine::init_mesh_pipeline()
{
    VkShaderModule vertShader, fragShader;
    if (!vkutil::load_shader_module("../shaders/colored_triangle_mesh.vert.spv", _device, &vertShader)) {
        fmt::println("Error when building the mesh vertex shader");
    }
    else {
        fmt::println("Mesh Vertex shader loaded successfully");
    }

    if (!vkutil::load_shader_module("../shaders/colored_triangle.frag.spv", _device, &fragShader)) {
        fmt::println("Error when building the mesh fragment shader");
    }
    else {
        fmt::println("Mesh Fragment shader loaded successfully");
    }
    
    // 设置push constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(GPUDrawPushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    // 创建管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_meshPipelineLayout));

    vkutil::PipelineBuilder builder;
    builder._pipelineLayout = _meshPipelineLayout;
    // 设置着色器
    builder.set_shaders(vertShader, fragShader);
    // 设置输入拓扑
    builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    // 设置多边形模式
    builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    // 设置剔除模式
    builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    // 设置多重采样为无
    builder.set_multisampling_none();
    // // 禁用深度和模板测试
    // builder.disable_depth_stencil();
    // 启用深度测试
    builder.enable_depth_test(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    // 禁用颜色混合
    builder.disable_blending();

    // 设置颜色附件格式
    builder.set_color_attachment_format(_drawImage.imageFormat);
    // 设置深度附件格式
    builder.set_depth_attachment_format(_depthImage.imageFormat);

    // 构建管线
    _meshPipeline = builder.build_pipeline(_device);

    // 释放着色器模块
    vkDestroyShaderModule(_device, vertShader, nullptr);
    vkDestroyShaderModule(_device, fragShader, nullptr);
    
    // 添加销毁函数到删除队列
    _mainDeletionQueue.push_function([this]() {
        vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
        vkDestroyPipeline(_device, _meshPipeline, nullptr);
    });
}

void VulkanEngine::init_default_data()
{
    // // 创建顶点数据
    // std::array<Vertex, 4> rect_vertices;

    // rect_vertices[0].position = glm::vec3(0.5f, -0.5f, 0.0f);
    // rect_vertices[1].position = glm::vec3(0.5f, 0.5f, 0.0f);
    // rect_vertices[2].position = glm::vec3(-0.5f, -0.5f, 0.0f);
    // rect_vertices[3].position = glm::vec3(-0.5f, 0.5f, 0.0f);
    
    // rect_vertices[0].color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    // rect_vertices[1].color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    // rect_vertices[2].color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    // rect_vertices[3].color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
    
    // std::array<uint32_t, 6> rect_indices = {0, 1, 2, 2, 1, 3};
    
    // _meshBuffers = upload_mesh(rect_indices, rect_vertices);
    
    // _mainDeletionQueue.push_function([this]() {
    //     destroy_buffer(_meshBuffers.indexBuffer);
    //     destroy_buffer(_meshBuffers.vertexBuffer);
    // });
    
    _testMeshes = load_gltf_files(this, "../assets/basicmesh.glb").value();
}

void VulkanEngine::init_imgui()
{
    // 创建imgui相关的descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 },
    };

    // 创建descriptor pool
    VkDescriptorPoolCreateInfo pool_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
    };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));
    
    // 初始化imgui库
    ImGui::CreateContext();

    // 初始化imgui的SDL2库
    ImGui_ImplSDL2_InitForVulkan(_window);

    ImGui_ImplVulkan_InitInfo init_info {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.QueueFamily = _graphicsQueueFamily;
    init_info.Queue = _graphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = VK_TRUE;
    
    // 动态渲染参数
    init_info.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
    };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;
    
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // 初始化imgui
    ImGui_ImplVulkan_Init(&init_info);
    
    // 创建字体纹理
    ImGui_ImplVulkan_CreateFontsTexture();

    // 添加销毁函数到删除队列
    _mainDeletionQueue.push_function([this, imguiPool]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiPool, nullptr);
    });
}

// 只分配空间，不填充数据
AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    AllocatedBuffer newBuffer;

    // 缓冲区创建信息
    VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
    };
    bufferInfo.size = allocSize;
    
    bufferInfo.usage = usage;
    
    VmaAllocationCreateInfo vmaAllocInfo {};
    vmaAllocInfo.usage = memoryUsage;
    // 设置内存映射标志为MAPPED，该标志表示内存分配后，内存会自动映射到CPU
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo,
     &newBuffer.buffer, &newBuffer.allocation, &newBuffer.allocationInfo));
    
    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

// 使用span避免数据拷贝
GPUMeshBuffers VulkanEngine::upload_mesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    // 计算索引和顶点的大小
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    
    GPUMeshBuffers newSurface;

    // 创建索引缓冲区
    newSurface.indexBuffer = create_buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
     VMA_MEMORY_USAGE_GPU_ONLY);
    
    // 创建顶点缓冲区，存储到SSBO中
    newSurface.vertexBuffer = create_buffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
     VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo bufferDeviceAddressInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
    };
    bufferDeviceAddressInfo.buffer = newSurface.vertexBuffer.buffer;
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &bufferDeviceAddressInfo);
    
    // 创建临时缓冲区，用于上传数据
    AllocatedBuffer stagingBuffer = create_buffer(indexBufferSize + vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
     VMA_MEMORY_USAGE_CPU_TO_GPU);

    void* data = stagingBuffer.allocation->GetMappedData();

    // 上传数据
    memcpy(data, indices.data(), indexBufferSize);
    memcpy(static_cast<char*>(data) + indexBufferSize, vertices.data(), vertexBufferSize);
    
    // 提交数据拷贝命令
    immediate_submit([&](VkCommandBuffer cmd) {
        // 拷贝索引数据
        VkBufferCopy indexCopy {};
        indexCopy.srcOffset = 0;
        indexCopy.dstOffset = 0;
        indexCopy.size = indexBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, newSurface.indexBuffer.buffer,
         1, &indexCopy);

        // 拷贝顶点数据
        VkBufferCopy vertexCopy {};
        vertexCopy.srcOffset = indexBufferSize;
        vertexCopy.dstOffset = 0;
        vertexCopy.size = vertexBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, newSurface.vertexBuffer.buffer,
         1, &vertexCopy);
    });

    // 销毁临时缓冲区
    destroy_buffer(stagingBuffer);

    return newSurface;  
}

void VulkanEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    for (auto imageView : _swapchainImageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
        // 按照创建的反序销毁并释放资源

        // GPU等待
        vkDeviceWaitIdle(_device);

        // 销毁command pool
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            // 销毁command pool
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            // 销毁栅栏和信号量
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
        }
        
        // 销毁网格数据
        for (auto& mesh : _testMeshes) {
            destroy_buffer(mesh->meshBuffers.indexBuffer);
            destroy_buffer(mesh->meshBuffers.vertexBuffer);
        }

        // 执行删除队列
        _mainDeletionQueue.flush();

        destroy_swapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debugMessenger, nullptr);
        vkDestroyInstance(_instance, nullptr);

        // 销毁SDL窗口，SDL是C语言库，需要手动销毁
        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    // 等待栅栏发出信号
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, VK_TRUE, static_cast<uint64_t>(1e9)));

    // 执行删除队列
    get_current_frame()._deletionQueue.flush();

    // 重置栅栏
    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence));

    // 获取交换链图像索引
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, static_cast<uint64_t>(1e9), get_current_frame()._swapchainSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

    // 获取command buffer
    VkCommandBuffer cmd = get_current_frame()._commandBuffer;

    // 重置command buffer
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // 设置drawImage的extent
    _drawImageExtent.width = _swapchainExtent.width;
    _drawImageExtent.height = _swapchainExtent.height;

    // 开始记录命令
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // 将绘制图像从未定义状态转换为通用状态
    vkutil::transition_image_layout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // 绘制背景
    draw_background(cmd);
    
    // 将绘制图像从通用状态转换为颜色附件状态
    vkutil::transition_image_layout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image_layout(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    
    // 绘制几何体
    draw_geometry(cmd);

    // 将绘制图像从颜色附件状态转换为复制源状态
    vkutil::transition_image_layout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // 将交换链图像从未定义状态转换为复制目标状态
    vkutil::transition_image_layout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 复制绘制图像到交换链图像
    vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawImageExtent, _swapchainExtent);

    // 将交换链图像从复制目标状态转换为颜色附件状态
    vkutil::transition_image_layout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    // 绘制imgui
    draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);
    
    // 将交换链图像从颜色附件状态转换为呈现源状态
    vkutil::transition_image_layout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // 结束记录命令
    VK_CHECK(vkEndCommandBuffer(cmd));

    // 提交命令
    VkCommandBufferSubmitInfo cmdSubmitInfo = vkinit::command_buffer_submit_info(cmd);

    // 等待交换链图像，stage为颜色附件输出阶段
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info( VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, get_current_frame()._swapchainSemaphore);

    // 信号渲染完成，stage为所有图形阶段
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info( VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame()._renderSemaphore);

    // 创建提交信息
    VkSubmitInfo2 submitInfo = vkinit::submit_info(&cmdSubmitInfo,  &signalInfo,  &waitInfo);

    // 提交命令
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, get_current_frame()._renderFence));

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
    };

    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    ++_frameNumber;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
    // // 绘制背景
    // VkClearColorValue clearValue{};
    // float flash = std::abs(std::sin(_frameNumber / 120.f));
    // clearValue =  { flash, flash, flash, 1.0f };

    // // 设置清除区域
    // VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    // // 清除图像
    // vkCmdClearColorImage(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
    
    // 绑定计算管线
    const auto&pipeline = _backgroundPipelines[_currentBackgroundPipeline];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipeline);

    // 绑定描述符集 
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.pipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);
    
    // 设置push constants
    vkCmdPushConstants(cmd, pipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &pipeline.pushConstants);

    // 执行计算
    vkCmdDispatch(cmd, std::ceil(_drawImageExtent.width / 16.0), std::ceil(_drawImageExtent.height / 16.0), 1);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    // 设置视图矩阵，将相机移动到z轴负5的位置
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -5.0f));
    // 设置投影矩阵，使用透视投影，视角为70度，近平面为10000，远平面为0.1
    // 深度值为1时表示近平面，0时表示远平面，反转深度，提高深度精度
    glm::mat4 projection = glm::perspective(glm::radians(70.0f), _drawImageExtent.width / static_cast<float>(_drawImageExtent.height), 10000.f, 0.1f);
    // 反转y轴，vulkan中y轴向上，而glm中y轴向下
    projection[1][1] *= -1;
    
    // 设置颜色附件信息
    VkRenderingAttachmentInfo colorAttachmentInfo = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // 设置深度附件信息
    VkRenderingAttachmentInfo depthAttachmentInfo = vkinit::attachment_info(_depthImage.imageView, nullptr, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // 设置渲染信息
    VkRenderingInfo renderingInfo = vkinit::rendering_info(_drawImageExtent, &colorAttachmentInfo, &depthAttachmentInfo);
    
    // 开始渲染
    vkCmdBeginRendering(cmd, &renderingInfo);

    // 绑定几何体管线
    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);

    // 设置viewport
    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_drawImageExtent.width);
    viewport.height = static_cast<float>(_drawImageExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // 设置裁剪矩形
    VkRect2D scissor {};
    scissor.offset = {0, 0};
    scissor.extent = _drawImageExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // 绘制三角形
    // vkCmdDraw(cmd, 3, 1, 0, 0);
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    GPUDrawPushConstants drawPushConstants;
    drawPushConstants.worldMatrix = projection * view;
    // drawPushConstants.vertexBuffer = _meshBuffers.vertexBufferAddress;
    drawPushConstants.vertexBuffer = _testMeshes[2]->meshBuffers.vertexBufferAddress;

    vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &drawPushConstants);
    vkCmdBindIndexBuffer(cmd, _testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    
    vkCmdDrawIndexed(cmd, _testMeshes[2]->surfaces[0].indexCount, 1, 
        _testMeshes[2]->surfaces[0].startIndex, 0, 0);

    // 结束渲染
    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    // 设置颜色附件信息
    VkRenderingAttachmentInfo colorAttachmentInfo = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // 设置渲染信息
    VkRenderingInfo renderingInfo = vkinit::rendering_info(_swapchainExtent, &colorAttachmentInfo, nullptr);
    
    // 动态渲染

    vkCmdBeginRendering(cmd, &renderingInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

// todo1: 不使用immediate_submit
// todo2: 使用一个新的queue，而不是使用_graphicsQueue
void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function)
{
    // 重置栅栏，此处不需要等待栅栏再重置，因为imgui的栅栏是独立的
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    // 重置命令池
    VK_CHECK(vkResetCommandPool(_device, _immCmdPool, 0));

    VkCommandBuffer cmd = _immCmdBuffer;

    // 开始记录命令
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // 执行函数
    function(cmd);

    // 结束记录命令
    VK_CHECK(vkEndCommandBuffer(cmd));

    // 提交命令
    VkCommandBufferSubmitInfo cmdSubmitInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submitInfo = vkinit::submit_info(&cmdSubmitInfo, nullptr, nullptr);
    
    // 提交命令，_renderFence将会被阻塞直到命令完成
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immFence));

    // 等待栅栏
    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, VK_TRUE, UINT64_MAX));
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }

            // 处理imgui事件
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 开始imgui渲染
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        if (ImGui::Begin("Background")) {
            const auto&pipeline = _backgroundPipelines[_currentBackgroundPipeline];

            ImGui::Text("Selected effect: %s", pipeline.name);

            ImGui::SliderInt("Effect Index: ", &_currentBackgroundPipeline, 0, _backgroundPipelines.size() - 1);
            
            ImGui::InputFloat4("Push Constant vec1: ", const_cast<float*>(glm::value_ptr(pipeline.pushConstants.data1)));
            ImGui::InputFloat4("Push Constant vec2: ", const_cast<float*>(glm::value_ptr(pipeline.pushConstants.data2)));
            ImGui::InputFloat4("Push Constant vec3: ", const_cast<float*>(glm::value_ptr(pipeline.pushConstants.data3)));
            ImGui::InputFloat4("Push Constant vec4: ", const_cast<float*>(glm::value_ptr(pipeline.pushConstants.data4)));

        }
        ImGui::End();

        // 渲染imgui
        ImGui::Render();

        draw();
    }
}