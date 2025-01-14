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

class PipelineBuilder {
public:
    PipelineBuilder();

    /**
     * @brief 清空渲染管线构建器
     */
    void clear();

    /**
     * @brief 构建渲染管线
     * @param device vulkan逻辑设备
     * @return 渲染管线
     */
    VkPipeline build_pipeline(VkDevice device);
    
    /**
     * @brief 设置着色器
     * @param vertShader 顶点着色器
     * @param fragShader 片段着色器
     */
    void set_shaders(VkShaderModule vertShader, VkShaderModule fragShader);
    
    /**
     * @brief 设置输入拓扑
     * @param topology 输入拓扑
     */
    void set_input_topology(VkPrimitiveTopology topology);
    
    /**
     * @brief 设置多边形模式
     * @param polygonMode 多边形模式
     */
    void set_polygon_mode(VkPolygonMode polygonMode);
    
    /**
     * @brief 设置剔除模式
     * @param cullMode 剔除模式
     */
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    
    /**
     * @brief 设置多重采样为无
     */
    void set_multisampling_none();

    /**
     * @brief 禁用颜色混合
     */
    void disable_blending();
    
    /**
     * @brief 设置颜色附件格式
     * @param format 颜色附件格式
     */
    void set_color_attachment_format(VkFormat format);
    
    /**
     * @brief 设置深度附件格式
     * @param format 深度附件格式
     */
    void set_depth_attachment_format(VkFormat format);
    
    /**
     * @brief 禁用深度和模板测试
     */
    void disable_depth_stencil();
    
    /**
     * @brief 启用深度测试
     * @param depthWriteEnable 是否启用深度写入
     * @param compareOp 深度比较操作
     */
    void enable_depth_test(bool depthWriteEnable, VkCompareOp compareOp);

public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
    
    // 输入装配，用于指定顶点数据如何装配到图元
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    // 光栅化，用于指定如何将图元转换为像素
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    // 颜色混合，用于指定如何将颜色混合到帧缓冲区
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    // 多重采样，用于指定如何对像素进行多重采样
    VkPipelineMultisampleStateCreateInfo _multisampling;
    // 管道布局，用于指定管道的布局
    VkPipelineLayout _pipelineLayout;
    // 深度测试，用于指定如何进行深度测试
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    // 渲染信息，用于动态渲染
    VkPipelineRenderingCreateInfo _renderingInfo;
    // 颜色格式，用于指定颜色格式
    VkFormat _colorFormat;
};

};