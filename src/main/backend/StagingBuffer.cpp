#include "StagingBuffer.h"

#include "../util/Logger.h"

std::pair<vma::UniqueBuffer, vma::UniqueAllocation> StagingBuffer::upload(
        const void *data, size_t size, vk::BufferUsageFlags usage
) {
    auto pair = mAllocator.createBufferUnique(
            {.size = size, .usage = usage | vk::BufferUsageFlagBits::eTransferDst},
            {.usage = vma::MemoryUsage::eAuto, .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal}
    );
    upload(data, size, *pair.first);
    return std::move(pair);
}
void StagingBuffer::upload(const void *data, size_t size, const vk::Buffer &dst) {
    vma::AllocationInfo result_info;
    auto [buf, alloc] = mAllocator.createBuffer(
            {
                .size = size,
                .usage = vk::BufferUsageFlagBits::eTransferSrc,
            },
            {
                .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                .usage = vma::MemoryUsage::eAuto,
                .requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            },
            &result_info
    );
    mAllocations.push_back({buf, alloc});

    std::memcpy(result_info.pMappedData, data, size);

    mCommands.copyBuffer(buf, dst, {vk::BufferCopy{.size = size}});
}

StagingBuffer::StagingBuffer(const vma::Allocator &allocator, const vk::Device &device, const vk::CommandPool &cmd_pool)
    : mDevice(device), mAllocator(allocator), mCommandPool(cmd_pool) {
    createCommandBuffer();
}

StagingBuffer::~StagingBuffer() {
    if (mAllocations.size() > 0) {
        Logger::fatal("Staging buffer destroyed with open allocations!");
    }
    mCommands.end();
    mDevice.freeCommandBuffers(mCommandPool, {mCommands});
}

void StagingBuffer::submit(const vk::Queue &queue) {
    mCommands.end();

    auto fence = mDevice.createFenceUnique(vk::FenceCreateInfo());
    queue.submit({vk::SubmitInfo().setCommandBuffers(mCommands)}, *fence);

    while (mDevice.waitForFences(*fence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }
    mDevice.freeCommandBuffers(mCommandPool, {mCommands});
    createCommandBuffer();

    for (const auto &[buffer, alloc]: mAllocations) {
        mAllocator.destroyBuffer(buffer, alloc);
    }
    mAllocations.clear();
}

void StagingBuffer::createCommandBuffer() {
    mCommands = mDevice.allocateCommandBuffers({.commandPool = mCommandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = 1})
                        .at(0);
    mCommands.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
}
