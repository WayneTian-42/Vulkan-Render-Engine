
#pragma once 

#include <vulkan/vulkan.h>

namespace vkutil {

    /**
     * @brief 转换图像布局
     * @param cmd 命令缓冲区
     * @param image 图像
     * @param oldLayout 旧布局
     * @param newLayout 新布局
     */
    void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

    /**
     * @brief 复制图像到图像
     * @param cmd 命令缓冲区
     * @param srcImage 源图像
     * @param dstImage 目标图像
     * @param srcSize 源图像大小
     * @param dstSize 目标图像大小
     */
    void copy_image_to_image(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, VkExtent2D srcSize, VkExtent2D dstSize);

    /**
     * @brief 生成mipmap
     * @param cmd 命令缓冲区
     * @param image 图像
     * @param imageSize 图像大小
     */
    void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);
};