#pragma once 
#include <vk_types.h>

namespace vkutil {

/**
 * @brief 从文件加载着色器模块
 * @param filePath 着色器文件路径
 * @param device 设备
 * @param outShaderModule 输出着色器模块
 * @return 是否成功，成功返回true，失败返回false
 */
bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);

};