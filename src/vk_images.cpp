#include <vk_images.h>
#include <vk_initializers.h>


void vkutil::transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 imageMemoryBarrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
    };

    // todo: 可以针对具体的图像类型进行优化
    imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageMemoryBarrier.oldLayout = oldLayout;
    imageMemoryBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
    imageMemoryBarrier.image = image;

    VkDependencyInfo dependencyInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
    };

    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void vkutil::copy_image_to_image(VkCommandBuffer cmd, VkImage srcImage, VkImage dstImage, VkExtent2D srcSize, VkExtent2D dstSize)
{
    VkImageBlit2 blitRegion {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = nullptr,
    };

    blitRegion.srcOffsets[1].x = srcSize.width;
    blitRegion.srcOffsets[1].y = srcSize.height;
    blitRegion.srcOffsets[1].z = 1;

    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = 1;

    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.mipLevel = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;

    VkBlitImageInfo2 blitInfo {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = nullptr,
    };  
    blitInfo.dstImage = dstImage;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.srcImage = srcImage;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.filter = VK_FILTER_LINEAR;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;

    vkCmdBlitImage2(cmd, &blitInfo);
}

void vkutil::generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize)
{
    // 计算mipmap层级数，根据图像的最大边长计算
    int mipLevels = static_cast<int>(std::floor(std::log2(std::max(imageSize.width, imageSize.height)))) + 1;

    // 遍历每个mipmap层级
    for (int mip = 0; mip < mipLevels; mip++) {
        
        // 计算当前mipmap层级的图像大小
        VkExtent2D mipSize = imageSize;
        mipSize.width /= 2;
        mipSize.height /= 2;

        // 创建图像内存屏障，用于同步图像布局转换
        VkImageMemoryBarrier2 imageMemoryBarrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext = nullptr,
        };

        // 设置内存屏障的源和目标访问掩码和管线阶段

        // 源阶段掩码设置为所有命令，因为我们不确定之前的命令在哪个阶段写入了图像
        imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        // 源访问掩码设置为写入，因为我们需要等待之前的写入操作完成
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        // 目标阶段掩码设置为所有命令，因为后续的blit操作可能在任何阶段执行
        imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        // 目标访问掩码设置为读写，因为后续的blit操作需要读取源图像并写入目标图像
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

        // 设置图像布局转换
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        // 设置图像子资源范围
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.subresourceRange.baseMipLevel = mip;
        imageMemoryBarrier.image = image;

        // 创建依赖信息
        VkDependencyInfo dependencyInfo {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
        };

        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;

        // 执行管线屏障
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);

        // 如果不是最后一个mipmap层级，则生成下一个层级
        if (mip < mipLevels - 1) {
            // blit操作能够将一个图像的子区域复制到另一个图像的子区域
            // 这里将当前mipmap层级的图像复制到下一个层级

            // 创建blit区域
            VkImageBlit2 blitRegion {
                .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                .pNext = nullptr,
            };

            // 设置源图像和目标图像的偏移
            blitRegion.srcOffsets[1].x = imageSize.width;
            blitRegion.srcOffsets[1].y = imageSize.height;
            blitRegion.srcOffsets[1].z = 1;

            blitRegion.dstOffsets[1].x = mipSize.width;
            blitRegion.dstOffsets[1].y = mipSize.height;
            blitRegion.dstOffsets[1].z = 1;

            // 设置源图像子资源
            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.srcSubresource.mipLevel = mip;

            // 设置目标图像子资源
            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstSubresource.mipLevel = mip + 1;

            // 创建blit信息
            VkBlitImageInfo2 blitInfo {
                .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                .pNext = nullptr,
            };

            // 设置blit操作的源图像和目标图像
            // 源图像和目标图像相同，但是在不同的mipmap层级，目的是将当前mipmap层级的图像缩放后复制到下一个层级
            blitInfo.srcImage = image;
            blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            blitInfo.dstImage = image;
            blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            blitInfo.filter = VK_FILTER_LINEAR;  // 使用线性过滤进行缩放
            blitInfo.regionCount = 1;
            blitInfo.pRegions = &blitRegion;

            // 执行blit操作
            vkCmdBlitImage2(cmd, &blitInfo);

            // 更新图像大小为下一个mipmap层级的大小
            imageSize = mipSize;
        }
    }

    // 将最终图像布局转换为着色器只读布局
    transition_image_layout(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}


