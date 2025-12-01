#include "StagingBuffer.h"

#include "../util/Logger.h"

std::pair<vma::UniqueBuffer, vma::UniqueAllocation> StagingBuffer::upload(
        const void *data, size_t size, vk::BufferUsageFlags usage
) {
    if (size == 0) {
        Logger::warning("Creating staging buffer with zero size, using dummy element instead.");
        size = 4;
        uint32_t dummy_element = 0;
        data = &dummy_element;
    }
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
    mAllocations.emplace_back(buf, alloc);

    std::memcpy(result_info.pMappedData, data, size);

    mCommands.copyBuffer(buf, dst, {vk::BufferCopy{.size = size}});
}

vk::Buffer StagingBuffer::stage(const void *data, size_t size) {
    auto [buf, ptr] = stage(size);
    std::memcpy(ptr, data, size);
    return buf;
}

std::pair<vk::Buffer, void*> StagingBuffer::stage(size_t size) {
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

    mAllocations.emplace_back(buf, alloc);

    return { buf, result_info.pMappedData };
}

StagingBuffer::StagingBuffer(const vma::Allocator &allocator, const vk::Device &device, const vk::CommandPool &cmd_pool)
    : mDevice(device), mAllocator(allocator), mCommandPool(cmd_pool) {
    createCommandBuffer();
}

StagingBuffer::~StagingBuffer() {
    if (!mAllocations.empty()) {
        Logger::fatal("Staging buffer destroyed with open allocations!");
    }
    if (mCommands) {
        mCommands.end();
        mDevice.freeCommandBuffers(mCommandPool, {mCommands});
    }
}

StagingBuffer::StagingBuffer(StagingBuffer &&other) noexcept
    : mDevice(std::exchange(other.mDevice, {})),
      mAllocator(std::exchange(other.mAllocator, {})),
      mCommandPool(std::exchange(other.mCommandPool, {})),
      mCommands(std::exchange(other.mCommands, {})),
      mAllocations(std::move(other.mAllocations)) {}

StagingBuffer &StagingBuffer::operator=(StagingBuffer &&other) noexcept {
    if (this == &other)
        return *this;
    mDevice = std::exchange(other.mDevice, {});
    mAllocator = std::exchange(other.mAllocator, {});
    mCommandPool = std::exchange(other.mCommandPool, {});
    mCommands = std::exchange(other.mCommands, {});
    mAllocations = std::move(other.mAllocations);
    return *this;
}

void StagingBuffer::submit(const vk::Queue &queue, const vk::SubmitInfo &submit_info_) {
    mCommands.end();

    vk::SubmitInfo submit_info = submit_info_;
    submit_info.setCommandBuffers(mCommands);
    auto fence = mDevice.createFenceUnique(vk::FenceCreateInfo());
    queue.submit({submit_info}, *fence);

    while (mDevice.waitForFences(*fence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }
    mDevice.freeCommandBuffers(mCommandPool, {mCommands});
    createCommandBuffer();

    for (const auto &[buffer, alloc]: mAllocations) {
        mAllocator.destroyBuffer(buffer, alloc);
    }
    mAllocations.clear();
}

void StagingBuffer::submitUnsynchronized(const vk::Queue &queue, const vk::SubmitInfo &submit_info_) {
    mCommands.end();

    vk::SubmitInfo submit_info = submit_info_;
    submit_info.setCommandBuffers(mCommands);
    queue.submit({submit_info});
}

void StagingBuffer::beginUnsynchronized() {
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
