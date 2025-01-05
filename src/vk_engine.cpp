//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

#include <VkBootstrap.h>

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
}

void VulkanEngine::init_swapchain()
{
    create_swapchain(_windowExtent.width, _windowExtent.height);
}

void VulkanEngine::init_commands()
{

}

void VulkanEngine::init_sync_structures()
{

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
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
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