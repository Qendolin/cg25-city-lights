#pragma once

#include <vulkan/vulkan.hpp>

#include "../util/static_vector.h"

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
    static const ImageResourceAccess FragmentShaderRead;
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
    ImageResourceAccess mPrevAccess = {};

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
    );
};

/// <summary>
/// Represents an image attachment, which is a combination of a vk::Image and a vk::ImageView.
/// </summary>
struct Attachment : ImageResource {
    vk::Image image = {};
    vk::ImageView view = {};
    vk::Format format = {};
    vk::ImageSubresourceRange range = {};

    /// <summary>
    /// Inserts an image memory barrier for the attachment.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier to.</param>
    /// <param name="begin">The resource access state at the beginning of the barrier.</param>
    /// <param name="end">The resource access state at the end of the barrier.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end);

    /// <summary>
    /// Inserts an image memory barrier for the attachment, transitioning from the previous state to a new one.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier to.</param>
    /// <param name="single">The new resource access state.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single);

    /// <summary>
    /// Checks if the attachment is valid (i.e., has a valid image and view).
    /// </summary>
    explicit operator bool() const { return image && view; }
};

/// <summary>
/// Configuration for dynamic rendering with a framebuffer.
/// </summary>
struct FramebufferRenderingConfig {
    vk::RenderingFlags flags = {};
    uint32_t layerCount = 1;
    uint32_t viewMask = 0;

    util::static_vector<bool, 32> enabledColorAttachments = {};
    bool enableDepthAttachment = true;
    bool enableStencilAttachment = true;
    util::static_vector<vk::AttachmentLoadOp, 32> colorLoadOps = {};
    util::static_vector<vk::AttachmentStoreOp, 32> colorStoreOps = {};
    vk::AttachmentLoadOp depthLoadOp = vk::AttachmentLoadOp::eLoad;
    vk::AttachmentStoreOp depthStoreOp = vk::AttachmentStoreOp::eStore;
    vk::AttachmentLoadOp stencilLoadOp = vk::AttachmentLoadOp::eLoad;
    vk::AttachmentStoreOp stencilStoreOp = vk::AttachmentStoreOp::eStore;

    util::static_vector<vk::ClearColorValue, 32> clearColors = {};
    float clearDepth = 1.0f;
    uint32_t clearStencil = 0;

    consteval static util::static_vector<bool, 32> all(const bool enabled) {
        std::array<bool, 32> arr{};
        std::ranges::fill(arr.begin(), arr.end(), enabled);
        return arr;
    }

    consteval static util::static_vector<vk::AttachmentLoadOp, 32> all(const vk::AttachmentLoadOp load_op) {
        std::array<vk::AttachmentLoadOp, 32> arr{};
        std::ranges::fill(arr.begin(), arr.end(), load_op);
        return arr;
    }

    consteval static util::static_vector<vk::AttachmentStoreOp, 32> all(const vk::AttachmentStoreOp store_op) {
        std::array<vk::AttachmentStoreOp, 32> arr{};
        std::ranges::fill(arr.begin(), arr.end(), store_op);
        return arr;
    }
};

/// <summary>
/// Represents a collection of attachments for rendering, used with Vulkan's dynamic rendering feature.
/// </summary>
/// <remarks>
/// With the dynamic rendering extension, VkFramebuffer isn't used.
/// But the concept of a framebuffer (a collection of attachments) is still useful.
/// </remarks>
class Framebuffer {
public:
    util::static_vector<Attachment, 32> colorAttachments = {};
    Attachment depthAttachment = {};
    Attachment stencilAttachment = {};

    /// <summary>
    /// Creates a vk::RenderingInfo struct for dynamic rendering.
    /// </summary>
    /// <param name="area">The rendering area.</param>
    /// <param name="config">The configuration for the rendering pass.</param>
    /// <returns>A configured vk::RenderingInfo struct.</returns>
    vk::RenderingInfo renderingInfo(const vk::Rect2D &area, const FramebufferRenderingConfig &config = {});

private:
    std::array<vk::RenderingAttachmentInfo, 32> mColorAttachmentInfos = {};
    vk::RenderingAttachmentInfo mDepthAttachmentInfo = {};
    vk::RenderingAttachmentInfo mStencilAttachmentInfo = {};
};
