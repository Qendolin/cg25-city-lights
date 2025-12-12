#include "BufferResource.h"

constexpr BufferResourceAccess BufferResourceAccess::TransferWrite = {
    .stage = vk::PipelineStageFlagBits2::eTransfer,
    .access = vk::AccessFlagBits2::eTransferWrite,
};

constexpr BufferResourceAccess BufferResourceAccess::TransferRead = {
    .stage = vk::PipelineStageFlagBits2::eTransfer,
    .access = vk::AccessFlagBits2::eTransferRead,
};

constexpr BufferResourceAccess BufferResourceAccess::ComputeShaderWrite = {
    .stage = vk::PipelineStageFlagBits2::eComputeShader,
    .access = vk::AccessFlagBits2::eShaderWrite,
};

constexpr BufferResourceAccess BufferResourceAccess::ComputeShaderRead = {
    .stage = vk::PipelineStageFlagBits2::eComputeShader,
    .access = vk::AccessFlagBits2::eShaderRead,
};

constexpr BufferResourceAccess BufferResourceAccess::ComputeShaderStorageRead = {
    .stage = vk::PipelineStageFlagBits2::eComputeShader,
    .access = vk::AccessFlagBits2::eShaderStorageRead,
};

constexpr BufferResourceAccess BufferResourceAccess::ComputeShaderStorageWrite = {
    .stage = vk::PipelineStageFlagBits2::eComputeShader,
    .access = vk::AccessFlagBits2::eShaderStorageWrite,
};

constexpr BufferResourceAccess BufferResourceAccess::ComputeShaderStorageReadWrite = {
    .stage = vk::PipelineStageFlagBits2::eComputeShader,
    .access = vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eShaderStorageRead,
};

constexpr BufferResourceAccess BufferResourceAccess::IndirectCommandRead = {
    .stage = vk::PipelineStageFlagBits2::eDrawIndirect,
    .access = vk::AccessFlagBits2::eIndirectCommandRead,
};

constexpr BufferResourceAccess BufferResourceAccess::GraphicsShaderUniformRead = {
    .stage = vk::PipelineStageFlagBits2::eAllGraphics,
    .access = vk::AccessFlagBits2::eUniformRead,
};

constexpr BufferResourceAccess BufferResourceAccess::GraphicsShaderStorageRead = {
    .stage = vk::PipelineStageFlagBits2::eAllGraphics,
    .access = vk::AccessFlagBits2::eShaderStorageRead,
};

BufferResource::BufferResource(BufferResource &&other) noexcept : mPrevAccess(std::exchange(other.mPrevAccess, {})) {}

BufferResource &BufferResource::operator=(BufferResource &&other) noexcept {
    if (this == &other)
        return *this;
    mPrevAccess = std::exchange(other.mPrevAccess, {});
    return *this;
}

void BufferResource::barrier(
        vk::Buffer buffer,
        size_t offset,
        size_t size,
        const vk::CommandBuffer &cmd_buf,
        const BufferResourceAccess &begin,
        const BufferResourceAccess &end
) const {
    vk::BufferMemoryBarrier2 barrier{
        .srcStageMask = mPrevAccess.stage,
        .srcAccessMask = mPrevAccess.access,
        .dstStageMask = begin.stage,
        .dstAccessMask = begin.access,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .buffer = buffer,
        .offset = offset,
        .size = size,
    };

    cmd_buf.pipelineBarrier2({
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
    });
    mPrevAccess = end;
}

void BufferResource::transfer(
        vk::Buffer buffer, vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue
) const {
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
