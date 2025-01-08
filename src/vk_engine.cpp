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
    VkFormat imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    VkImageCreateInfo drawImageInfo = vkinit::image_create_info(imageFormat, drawImageUsages, imageExtent);

    // 分配图像内存
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VK_CHECK(vmaCreateImage(_allocator, &drawImageInfo, &allocationInfo, &_drawImage.image, &_drawImage.allocation, nullptr));

    // 创建图像视图
    VkImageViewCreateInfo imageViewInfo = vkinit::imageview_create_info(imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(_device, &imageViewInfo, nullptr, &_drawImage.imageView));

    // 添加图像销毁函数到删除队列
    _mainDeletionQueue.push_function([this]() {
        vkDestroyImageView(_device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
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

    VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_gradientPipelineLayout));

    // 加载着色器模块
    VkShaderModule computeDrawShader;
    if (!vkutil::load_shader_module("../shaders/gradient.comp.spv", _device, &computeDrawShader)) {
        fmt::println("Error when building the compute shader");
    }

    // 设置着色器阶段信息
    VkPipelineShaderStageCreateInfo shaderStageInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
    };
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = computeDrawShader;
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

    // 创建计算管线
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));

    // 释放着色器模块
    vkDestroyShaderModule(_device, computeDrawShader, nullptr);

    // 添加销毁函数到删除队列
    _mainDeletionQueue.push_function([this] {
        vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
        vkDestroyPipeline(_device, _gradientPipeline, nullptr);
    });
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
    // nothing yet

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

    // 将交换链图像从未定义状态转换为颜色附件优化状态
    vkutil::transition_image_layout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // 绘制背景
    draw_background(cmd);

    // 将交换链图像从颜色附件优化状态转换为呈现源状态
    vkutil::transition_image_layout(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image_layout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 复制图像到图像
    vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawImageExtent, _swapchainExtent);

    // 将交换链图像从颜色附件优化状态转换为呈现源状态
    vkutil::transition_image_layout(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);

    // 绑定描述符集 
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

    // 执行计算
    vkCmdDispatch(cmd, std::ceil(_drawImageExtent.width / 16.0), std::ceil(_drawImageExtent.height / 16.0), 1);
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
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}