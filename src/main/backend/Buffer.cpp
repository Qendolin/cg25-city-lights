#include "Buffer.h"


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
    vk::BufferMemoryBarrier2 src_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eNone,
        .srcAccessMask = {},
        .dstStageMask = vk::PipelineStageFlagBits2::eNone,
        .dstAccessMask = {},
        .srcQueueFamilyIndex = src_queue,
        .dstQueueFamilyIndex = dst_queue,
        .buffer = buffer,
        .offset = 0,
        .size = vk::WholeSize,
    };

    src_cmd_buf.pipelineBarrier2({
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &src_barrier,
    });
    vk::BufferMemoryBarrier2 dst_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eNone,
        .srcAccessMask = {},
        .dstStageMask = vk::PipelineStageFlagBits2::eNone,
        .dstAccessMask = {},
        .srcQueueFamilyIndex = src_queue,
        .dstQueueFamilyIndex = dst_queue,
        .buffer = buffer,
        .offset = 0,
        .size = vk::WholeSize,
    };

    dst_cmd_buf.pipelineBarrier2({
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &dst_barrier,
    });
}
