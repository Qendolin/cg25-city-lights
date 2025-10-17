#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "../util/static_vector.h"
#include "ImageResource.h"

/// <summary>
/// Represents an image attachment, which is a combination of a vk::Image and a vk::ImageView.
/// </summary>
struct Attachment : ImageResource {
    vk::Image image = {};
    vk::ImageView view = {};
    vk::Format format = {};
    vk::Extent2D extents = {};
    vk::ImageSubresourceRange range = {};

    /// <summary>
    /// Inserts an image memory barrier for the attachment.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier to.</param>
    /// <param name="begin">The resource access state at the beginning of the barrier.</param>
    /// <param name="end">The resource access state at the end of the barrier.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end) const;

    /// <summary>
    /// Inserts an image memory barrier for the attachment, transitioning from the previous state to a new one.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier to.</param>
    /// <param name="single">The new resource access state.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single) const;

    /// <summary>
    /// Checks if the attachment is valid (i.e., has a valid image and view).
    /// </summary>
    explicit operator bool() const { return image && view; }
};


/// <summary>
/// Represents an image attachment, which is a combination of a vk::Image and a vk::ImageView.
/// </summary>
class AttachmentImage {
    vma::UniqueImage mImage = {};
    vma::UniqueAllocation mImageAlloc = {};
    vk::UniqueImageView mView = {};
    vk::Format mFormat = {};
    vk::Extent2D mExtent = {};
    vk::ImageSubresourceRange mRange = {};

public:
    AttachmentImage(
            vma::UniqueImage &&image,
            vma::UniqueAllocation &&alloc,
            vk::UniqueImageView &&image_view,
            vk::Format format,
            const vk::Extent2D &extent,
            const vk::ImageSubresourceRange &range
    );

    AttachmentImage(const vma::Allocator &allocator, const vk::Device &device, vk::Format format, const vk::Extent2D& extent, vk::ImageUsageFlags usage_flags = {});

    operator Attachment() const { // NOLINT(*-explicit-constructor)
        return {
           .image = *mImage,
           .view = *mView,
           .format = mFormat,
           .extents = mExtent,
           .range = mRange
        };
    }

    /// <summary>
    /// Checks if the attachment is valid (i.e., has a valid image and view).
    /// </summary>
    explicit operator bool() const { return mImage && mView; }
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
    float clearDepth = 0.0f;
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

    Framebuffer() = default;

    explicit Framebuffer(const vk::Extent2D &extent) : mArea({.offset = {0, 0}, .extent = extent}) {}

    explicit Framebuffer(const vk::Rect2D &area) : mArea(area) {}

    /// <summary>
    /// Creates a vk::RenderingInfo struct for dynamic rendering.
    /// </summary>
    /// <param name="config">The configuration for the rendering pass.</param>
    /// <returns>A configured vk::RenderingInfo struct.</returns>
    [[nodiscard]] vk::RenderingInfo renderingInfo(const FramebufferRenderingConfig &config = {}) const;

    /// <summary>
    /// Returns the format of the depth attachment.
    /// </summary>
    /// <returns>The vk::Format of the depth attachment.</returns>
    [[nodiscard]] vk::Format depthFormat() const;

    /// <summary>
    /// Returns the format of the stencil attachment.
    /// </summary>
    /// <returns>The vk::Format of the stencil attachment.</returns>
    [[nodiscard]] vk::Format stencilFormat() const { return stencilAttachment.format; }

    /// <summary>
    /// Returns a vector of formats for all color attachments.
    /// </summary>
    /// <returns>A vector of vk::Format for all color attachments.</returns>
    [[nodiscard]] util::static_vector<vk::Format, 32> colorFormats() const {
        util::static_vector<vk::Format, 32> result;
        for (const auto &attachment: colorAttachments) {
            result.push_back(attachment.format);
        }
        return result;
    }

    /// <returns>The rendering area.</returns>
    [[nodiscard]] vk::Rect2D area() const { return mArea; }

    /// <returns>The extents of the rendering area.</returns>
    [[nodiscard]] vk::Extent2D extent() const { return mArea.extent; }

    /// <summary>
    /// Returns a Vulkan viewport that covers the framebuffer's area, with the y-axis flipped to match OpenGL conventions.
    /// </summary>
    [[nodiscard]] vk::Viewport viewport(bool flip_y) const;

private:
    vk::Rect2D mArea;
    mutable std::array<vk::RenderingAttachmentInfo, 32> mColorAttachmentInfos = {};
    mutable vk::RenderingAttachmentInfo mDepthAttachmentInfo = {};
    mutable vk::RenderingAttachmentInfo mStencilAttachmentInfo = {};
};
