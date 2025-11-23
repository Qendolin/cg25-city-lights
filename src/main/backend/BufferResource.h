#pragma once

#include <vulkan/vulkan.hpp>

/// <summary>
/// Defines pipeline stage, access type, and buffer layout for an buffer resource. This is used for creating buffer memory barriers.
/// </summary>
/// <remarks>
/// There are a lot of possible buffer barrier variations, but I've found that only a small subset is actually useful in practice.
/// So I prefer simply defining all of them as constants when I need one. So far this worked out great in my projects.
/// </remarks>
struct BufferResourceAccess {
    vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eTopOfPipe;
    vk::AccessFlags2 access = vk::AccessFlagBits2::eNone;

    static const BufferResourceAccess TransferWrite;
    static const BufferResourceAccess TransferRead;
    static const BufferResourceAccess ComputeShaderWrite;
    static const BufferResourceAccess ComputeShaderRead;
    static const BufferResourceAccess ComputeShaderStorageWrite;
    static const BufferResourceAccess ComputeShaderStorageRead;
    static const BufferResourceAccess ComputeShaderStorageReadWrite;
    static const BufferResourceAccess IndirectCommandRead;
    static const BufferResourceAccess GraphicsShaderUniformRead;
};

/// <summary>
/// Base class for buffer resources that handles buffer memory barriers.
/// </summary>
class BufferResource {
protected:
    mutable BufferResourceAccess mPrevAccess = {};

    /// <summary>
    /// Inserts an buffer memory barrier into the command buffer.
    /// </summary>
    /// <param name="buffer">The buffer to which the barrier applies.</param>
    /// <param name="offset">The start of the access range.</param>
    /// <param name="size">The size of the access range.</param>
    /// <param name="cmd_buf">The command buffer to record the barrier to.</param>
    /// <param name="begin">The resource access state at the beginning of the barrier.</param>
    /// <param name="end">The resource access state at the end of the barrier.</param>
    void barrier(
            vk::Buffer buffer,
            size_t offset,
            size_t size,
            const vk::CommandBuffer &cmd_buf,
            const BufferResourceAccess &begin,
            const BufferResourceAccess &end
    ) const;

    /// <summary>
    /// Transfers ownership of the buffer between queue families.
    /// It does NOT perform any memory barriers. Execution ordering must be handled with a semaphore.
    /// </summary>
    /// <param name="buffer">The buffer to which the transfer applies.</param>
    /// <param name="src_cmd_buf">The command buffer in the source queue to record the barrier into.</param>
    /// <param name="dst_cmd_buf">The command buffer in the destination queue to record the barrier into.</param>
    /// <param name="src_queue">The index of the source queue family.</param>
    /// <param name="dst_queue">The index of the destination queue family.</param>
    void transfer(vk::Buffer buffer, vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue) const {
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

};
