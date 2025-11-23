#include "Buffer.h"

#include "../debug/Annotation.h"
#include "../util/Logger.h"
#include "../util/math.h"


Buffer::Buffer(vma::UniqueBuffer &&buffer, vma::UniqueAllocation &&allocation, size_t size)
    : size(size), buffer(*buffer), mBuffer(std::move(buffer)), mAllocation(std::move(allocation)) {}

Buffer Buffer::create(const vma::Allocator &allocator, const BufferCreateInfo &create_info) {
    auto [buffer, alloc] = allocator.createBufferUnique(
            {
                .size = create_info.size,
                .usage = create_info.usage,
            },
            {
                .flags = create_info.flags,
                .usage = create_info.device,
                .requiredFlags = create_info.requiredFlags,
                .preferredFlags = create_info.preferredFlags,
            }
    );
    return Buffer(std::move(buffer), std::move(alloc), create_info.size);
}

void Buffer::barrier(const vk::CommandBuffer &cmd_buf, const BufferResourceAccess &begin, const BufferResourceAccess &end) {
    BufferResource::barrier(*mBuffer, 0, vk::WholeSize, cmd_buf, begin, end);
}

void Buffer::barrier(const vk::CommandBuffer &cmd_buf, const BufferResourceAccess &single) {
    barrier(cmd_buf, single, single);
}

void Buffer::transfer(vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue) {
    BufferResource::transfer(*mBuffer, src_cmd_buf, dst_cmd_buf, src_queue, dst_queue);
}

void BufferRef::barrier(const vk::CommandBuffer &cmd_buf, const BufferResourceAccess &begin, const BufferResourceAccess &end) {
    BufferResource::barrier(buffer, 0, vk::WholeSize, cmd_buf, begin, end);
}

void BufferRef::barrier(const vk::CommandBuffer &cmd_buf, const BufferResourceAccess &single) {
    barrier(cmd_buf, single, single);
}

void BufferRef::transfer(vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue) {
    BufferResource::transfer(buffer, src_cmd_buf, dst_cmd_buf, src_queue, dst_queue);
}

struct TransientBufferAllocatorImpl {
    vk::Device mDevice;
    vma::Allocator mAllocator;

    vk::Buffer mBackingBuffer;
    vma::Allocation mBackingAlloc;
    vk::DeviceSize mTotalSize;
    vk::DeviceSize mCurrentOffset = 0;
    std::vector<vk::Buffer> mAliases;

    struct Dedicated {
        vk::Buffer buffer;
        vma::Allocation alloc;
    };
    std::vector<Dedicated> mDedicated;

    TransientBufferAllocatorImpl(const vk::Device& device, const vma::Allocator& allocator, vk::DeviceSize capacity)
        : mDevice(device), mAllocator(allocator), mTotalSize(capacity) {

        vk::BufferCreateInfo bufInfo = {};
        bufInfo.size = capacity;
        // Usage flags only need to ensure VMA picks a compatible memory type (Device Local)
        bufInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst;

        vma::AllocationCreateInfo allocInfo = {};
        allocInfo.usage = vma::MemoryUsage::eAuto;
        allocInfo.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;
        allocInfo.flags = vma::AllocationCreateFlagBits::eCanAlias;

        std::tie(mBackingBuffer, mBackingAlloc) = mAllocator.createBuffer(bufInfo, allocInfo);
        allocator.setAllocationName(mBackingAlloc, "transient_buffer_allocator_backing_allocation");
        util::setDebugName(mDevice, mBackingBuffer, "transient_buffer_allocator_backing_buffer");
        mAliases.reserve(64);
    }

    ~TransientBufferAllocatorImpl() {
        for (auto buf: mAliases)
            mAllocator.destroyBuffer(buf, nullptr);
        for (auto &d: mDedicated)
            mAllocator.destroyBuffer(d.buffer, d.alloc);
        mAllocator.destroyBuffer(mBackingBuffer, mBackingAlloc);
    }
};

BufferRef TransientBufferAllocator::allocate(vk::DeviceSize size, vk::BufferUsageFlags usage) const {
    assert(mImpl);

    vk::DeviceSize align = 256;
    vk::DeviceSize alignedOffset = util::alignOffset(mImpl->mCurrentOffset, align);

    // Handle oversized allocations or full ring buffer by creating a dedicated buffer
    if (alignedOffset + size > mImpl->mTotalSize) {
        Logger::warning(std::format(
                "Oversized transient buffer allocated: {} kB over {} kB limit.",
                util::divCeil(alignedOffset + size - mImpl->mTotalSize, 1024), mImpl->mTotalSize / 1024
        ));
        vk::BufferCreateInfo bufInfo = {};
        bufInfo.size = size;
        bufInfo.usage = usage;

        vma::AllocationCreateInfo allocInfo = {};
        allocInfo.usage = vma::MemoryUsage::eAuto;
        allocInfo.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;

        auto [buf, alloc] = mImpl->mAllocator.createBuffer(bufInfo, allocInfo);
        mImpl->mDedicated.push_back({buf, alloc});
        util::setDebugName(mImpl->mDevice, buf, "transient_buffer_dedicated_buffer");
        return BufferRef(buf, size);
    }

    // Create a lightweight alias into the pre-allocated backing buffer
    vk::BufferCreateInfo info = {};
    info.size = size;
    info.usage = usage;

    vk::Buffer alias = mImpl->mAllocator.createAliasingBuffer2(mImpl->mBackingAlloc, alignedOffset, info);
    util::setDebugName(mImpl->mDevice, alias, "transient_buffer_aliased_buffer");

    mImpl->mAliases.push_back(alias);
    mImpl->mCurrentOffset = alignedOffset + size;
    return BufferRef(alias, size);
}

void TransientBufferAllocator::reset() {
    assert(mImpl);

    for (auto buf: mImpl->mAliases) {
        mImpl->mAllocator.destroyBuffer(buf, nullptr);
    }
    mImpl->mAliases.clear();
    mImpl->mCurrentOffset = 0;

    for (auto &d: mImpl->mDedicated) {
        mImpl->mAllocator.destroyBuffer(d.buffer, d.alloc);
    }
    mImpl->mDedicated.clear();
}

UniqueTransientBufferAllocator::UniqueTransientBufferAllocator(const vk::Device& device, const vma::Allocator& allocator, vk::DeviceSize capacity) {
    mImpl = new TransientBufferAllocatorImpl(device, allocator, capacity);
}

UniqueTransientBufferAllocator::~UniqueTransientBufferAllocator() { delete mImpl; }

UniqueTransientBufferAllocator::UniqueTransientBufferAllocator(UniqueTransientBufferAllocator &&other) noexcept {
    mImpl = std::exchange(other.mImpl, nullptr);
}

UniqueTransientBufferAllocator &UniqueTransientBufferAllocator::operator=(UniqueTransientBufferAllocator &&other) noexcept {
    if (this != &other) {
        delete mImpl;
        mImpl = std::exchange(other.mImpl, nullptr);
    }
    return *this;
}
