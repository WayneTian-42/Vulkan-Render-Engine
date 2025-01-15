#include <vk_descriptors.h>

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newbind {};
    newbind.binding = binding;
    newbind.descriptorCount = 1;
    newbind.descriptorType = type;

    bindings.push_back(newbind);

}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = pNext,
    };

    info.pBindings = bindings.data();
    info.bindingCount = (uint32_t)bindings.size();
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * maxSets)
        });
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    };
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
    };

    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));

    return ds;
}


void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t initalSets, std::span<PoolSizeRatio> poolRatios)
{
    // 清空描述符池大小比例
    _poolRatios.clear();

    // 添加描述符池大小比例
    for (auto& ratio : poolRatios) {
        _poolRatios.push_back(ratio);
    }

    // 创建描述符池
    auto newPool = create_pool(device, initalSets, _poolRatios);

    // 设置每个描述符池的描述符集数量
    _setsPerPool = initalSets * 1.5;

    // 将新描述符池添加到可用池中
    _readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{
    // clear操作是针对所有池进行reset操作，操作后池中的描述符集会被释放，
    // 但是池本身不会被销毁，所以仍然可以用于分配描述符集
    for (auto pool : _readyPools) {
        vkResetDescriptorPool(device, pool, 0);
    }

    for (auto pool : _fullPools) {
        vkResetDescriptorPool(device, pool, 0);
        _readyPools.push_back(pool);
    }

    _fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pool(VkDevice device)
{
    // destroy操作是针对所有池进行销毁操作，操作后池中的描述符集会被释放，
    // 池本身也会被销毁，所以需要释放池
    for (auto pool : _readyPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    _readyPools.clear();

    for (auto pool : _fullPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    _fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext/* = nullptr*/)
{
    // 获取或生成一个描述符池用于分配描述符集
    auto poolToUse = get_pool(device);

    // 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = pNext,
    };
    // 设置描述符池
    allocInfo.descriptorPool = poolToUse;
    // 设置描述符集数量
    allocInfo.descriptorSetCount = 1;
    // 设置描述符集布局
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        // 如果描述符池已满，则将池添加到已满池中
        _fullPools.push_back(poolToUse);

        // 创建一个新的描述符池
        poolToUse = create_pool(device, _setsPerPool, _poolRatios);
        allocInfo.descriptorPool = poolToUse;

        // 重新分配描述符集，第二次失败后，会抛出异常
        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }
    _readyPools.push_back(poolToUse);

    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
    VkDescriptorPool newPool {};
    if (_readyPools.empty()) {
        // 如果可用池为空，则创建一个新的描述符池
        newPool = create_pool(device, _setsPerPool, _poolRatios);
        _readyPools.push_back(newPool);

        // 扩充描述符池大小，类似vector扩容
        _setsPerPool *= 1.5;
        // 限制描述符池大小
        if (_setsPerPool > 4096) {
            _setsPerPool = 4096;
        }
    } else {
        // 如果可用池不为空，则返回最后一个可用池
        newPool = _readyPools.back();
        // 移除最后一个可用池
        _readyPools.pop_back();
    }
    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
    // 创建描述符池容量信息
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto& ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            // 设置描述符类型
            .type = ratio.type,
            // 计算描述符的数量
            .descriptorCount = uint32_t(ratio.ratio * setCount)
        });
    }

    // 创建描述符池
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    };
    poolInfo.flags = 0;
    // 设置描述符池的最大描述符集数量
    poolInfo.maxSets = setCount;
    // 设置描述符池的容量信息
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    // 创建描述符池
    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool));
    return pool;
}

void DescriptorWriter::write_image(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout, VkDescriptorType type)
{
    // 创建图像描述符信息并添加到图像描述符信息队列中
    auto& imageInfo = imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = imageView,
        .imageLayout = imageLayout,
    });

    // 创建写入描述符信息
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
    };

    // 设置绑定索引
    write.dstBinding = binding;
    // 设置描述符集为空
    write.dstSet = VK_NULL_HANDLE;
    // 设置描述符数量
    write.descriptorCount = 1;
    // 设置描述符类型
    write.descriptorType = type;
    // 设置图像描述符信息
    write.pImageInfo = &imageInfo;

    writes.push_back(write);
}

void DescriptorWriter::write_buffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, VkDescriptorType type)
{
    // 创建缓冲区描述符信息并添加到缓冲区描述符信息队列中
    auto& bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = range,
    });

    // 创建写入描述符信息
    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
    };

    // 设置绑定索引
    write.dstBinding = binding;
    // 设置描述符集为空
    write.dstSet = VK_NULL_HANDLE;
    // 设置描述符数量
    write.descriptorCount = 1;
    // 设置描述符类型
    write.descriptorType = type;
    // 设置缓冲区描述符信息
    write.pBufferInfo = &bufferInfo;

    writes.push_back(write);
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    bufferInfos.clear();
    writes.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
    for (auto& write : writes) {
        write.dstSet = set;
    }

    // 更新描述符集
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
        writes.data(), 0, nullptr);
}
