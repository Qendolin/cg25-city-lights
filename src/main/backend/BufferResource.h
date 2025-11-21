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
    static const BufferResourceAccess IndirectCommandRead;
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
};
