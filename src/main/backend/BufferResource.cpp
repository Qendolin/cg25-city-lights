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

constexpr BufferResourceAccess BufferResourceAccess::IndirectCommandRead = {
    .stage = vk::PipelineStageFlagBits2::eDrawIndirect,
    .access = vk::AccessFlagBits2::eIndirectCommandRead,
};

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
