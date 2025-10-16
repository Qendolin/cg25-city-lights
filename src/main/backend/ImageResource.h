#pragma once

#include <vulkan/vulkan.hpp>

/// <summary>
/// Defines pipeline stage, access type, and image layout for an image resource. This is used for creating image memory barriers.
/// </summary>
/// <remarks>
/// There are a lot of possible image barrier variations, but I've found that only a small subset is actually useful in practice.
/// So I prefer simply defining all of them as constants when I need one. So far this worked out great in my projects.
/// </remarks>
struct ImageResourceAccess {
    vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eTopOfPipe;
    vk::AccessFlags2 access = vk::AccessFlagBits2::eNone;
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;

    static const ImageResourceAccess TransferWrite;
    static const ImageResourceAccess ComputeShaderWriteGeneral;
    static const ImageResourceAccess ComputeShaderReadGeneral;
    static const ImageResourceAccess ComputeShaderReadOptimal;
    static const ImageResourceAccess FragmentShaderReadOptimal;
    static const ImageResourceAccess ColorAttachmentWrite;
    static const ImageResourceAccess DepthAttachmentWrite;
    static const ImageResourceAccess DepthAttachmentRead;
    static const ImageResourceAccess PresentSrc;
};

/// <summary>
/// Base class for image resources that handles image memory barriers.
/// </summary>
class ImageResource {
protected:
    mutable ImageResourceAccess mPrevAccess = {};

    /// <summary>
    /// Inserts an image memory barrier into the command buffer.
    /// </summary>
    /// <param name="image">The image to which the barrier applies.</param>
    /// <param name="range">The subresource range of the image.</param>
    /// <param name="cmd_buf">The command buffer to record the barrier to.</param>
    /// <param name="begin">The resource access state at the beginning of the barrier.</param>
    /// <param name="end">The resource access state at the end of the barrier.</param>
    void barrier(
            vk::Image image,
            vk::ImageSubresourceRange range,
            const vk::CommandBuffer &cmd_buf,
            const ImageResourceAccess &begin,
            const ImageResourceAccess &end
    ) const;
};
