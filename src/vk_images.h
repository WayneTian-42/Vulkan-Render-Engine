
#pragma once 

#include <vulkan/vulkan.h>

namespace vkutil {

    void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
};