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
    static const ImageResourceAccess TransferRead;
    static const ImageResourceAccess ComputeShaderWriteGeneral;
    static const ImageResourceAccess ComputeShaderReadGeneral;
    static const ImageResourceAccess ComputeShaderReadOptimal;
    static const ImageResourceAccess FragmentShaderReadOptimal;
    static const ImageResourceAccess ColorAttachmentLoad;
    static const ImageResourceAccess ColorAttachmentWrite;
    static const ImageResourceAccess DepthAttachmentLateOps;
    static const ImageResourceAccess DepthAttachmentEarlyOps;
    static const ImageResourceAccess PresentSrc;
};

/// <summary>
/// Base class for image resources that handles image memory barriers.
/// </summary>
class ImageResource {
public:
    ImageResource() = default;
    virtual ~ImageResource() = default;

    ImageResource(const ImageResource &other) = delete;
    ImageResource &operator=(const ImageResource &other) = delete;

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

    /// <summary>
    /// Transfers ownership of the image between queue families.
    /// It does NOT perform any memory barriers or layout transitions. Execution ordering must be handled with a semaphore.
    /// </summary>
    /// <param name="image">The image to which the transfer applies.</param>
    /// <param name="range">The subresource range of the image.</param>
    /// <param name="src_cmd_buf">The command buffer in the source queue to record the barrier into.</param>
    /// <param name="dst_cmd_buf">The command buffer in the destination queue to record the barrier into.</param>
    /// <param name="src_queue">The index of the source queue family.</param>
    /// <param name="dst_queue">The index of the destination queue family.</param>
    void transfer(
            vk::Image image,
            vk::ImageSubresourceRange range,
            vk::CommandBuffer src_cmd_buf,
            vk::CommandBuffer dst_cmd_buf,
            uint32_t src_queue,
            uint32_t dst_queue
    ) const;

    ImageResource(ImageResource &&other) noexcept;
    ImageResource &operator=(ImageResource &&other) noexcept;
};
