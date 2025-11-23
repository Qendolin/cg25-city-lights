#include "Framebuffer.h"

#include <vulkan/utility/vk_format_utils.h>

#include "../util/Logger.h"

void Attachment::barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end) const {
    ImageResource::barrier(image, range, cmd_buf, begin, end);
}

void Attachment::barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single) const {
    barrier(cmd_buf, single, single);
}

void Attachment::setBarrierState(const ImageResourceAccess &last_access) const {
    mPrevAccess = last_access;
}

vk::Format Framebuffer::depthFormat() const { return depthAttachment.format; }

vk::Viewport Framebuffer::viewport(bool flip_y) const {
    if (flip_y) {
        return vk::Viewport{
            0.0f,
            static_cast<float>(mArea.extent.height),
            static_cast<float>(mArea.extent.width),
            -static_cast<float>(mArea.extent.height),
            0.0f,
            1.0f
        };
    } else {
        return vk::Viewport{0.0f, 0.0f, static_cast<float>(mArea.extent.width), static_cast<float>(mArea.extent.height),
                            0.0f, 1.0f};
    }
}

AttachmentImage::AttachmentImage(
        vma::UniqueImage &&image,
        vma::UniqueAllocation &&alloc,
        vk::UniqueImageView &&image_view,
        vk::Format format,
        const vk::Extent2D &extent,
        const vk::ImageSubresourceRange &range
)
    : mImage(std::move(image)),
      mImageAlloc(std::move(alloc)),
      mView(std::move(image_view)),
      mFormat(format),
      mExtent(extent),
      mRange(range) {}


AttachmentImage::AttachmentImage(const vma::Allocator &allocator, const vk::Device &device, vk::Format format, const vk::Extent2D& extent, vk::ImageUsageFlags usage_flags) : mFormat(format), mExtent(extent) {
    vk::ImageAspectFlags aspect_flags;

    if (vkuFormatIsColor(VkFormat(format))) {
        aspect_flags = vk::ImageAspectFlagBits::eColor;
        usage_flags |= vk::ImageUsageFlagBits::eColorAttachment;
    } else if (vkuFormatIsDepthOnly(VkFormat(format))) {
        aspect_flags = vk::ImageAspectFlagBits::eDepth;
        usage_flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    } else if (vkuFormatIsDepthAndStencil(VkFormat(format))) {
        aspect_flags = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        usage_flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    } else if (vkuFormatIsStencilOnly(VkFormat(format))) {
        aspect_flags = vk::ImageAspectFlagBits::eStencil;
        usage_flags |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    } else {
        Logger::fatal(std::format("Unsupported format: {}", vk::to_string(format)));
    }

    std::tie(mImage, mImageAlloc) = allocator.createImageUnique(
            {
                .imageType = vk::ImageType::e2D,
                .format = format,
                .extent = vk::Extent3D{.width = extent.width, .height = extent.height, .depth = 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = vk::ImageTiling::eOptimal,
                .usage = usage_flags,
                .initialLayout = vk::ImageLayout::eUndefined,
            },
            {
                .usage = vma::MemoryUsage::eAuto,
                .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
            }
    );
    mRange = {.aspectMask = aspect_flags, .levelCount = 1, .layerCount = 1};
    mView = device.createImageViewUnique({
        .image = *mImage,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = mRange,
    });
}

vk::RenderingInfo Framebuffer::renderingInfo(const FramebufferRenderingConfig &config) const {
    vk::RenderingInfo result = {
        .flags = config.flags,
        .renderArea = mArea,
        .layerCount = config.layerCount,
        .viewMask = config.viewMask,
    };

    for (size_t i = 0; i < colorAttachments.size(); i++) {
        const auto &attachment = colorAttachments[i];
        bool enabled = i < config.enabledColorAttachments.size() ? config.enabledColorAttachments[i] : true;
        auto clearColor = i < config.clearColors.size() ? config.clearColors[i] : vk::ClearColorValue{};
        auto loadOp = i < config.colorLoadOps.size() ? config.colorLoadOps[i] : vk::AttachmentLoadOp::eLoad;
        auto storeOp = i < config.colorStoreOps.size() ? config.colorStoreOps[i] : vk::AttachmentStoreOp::eStore;
        if (attachment && enabled) {
            mColorAttachmentInfos[i] = {
                .imageView = attachment.view,
                .imageLayout = vk::ImageLayout::eAttachmentOptimal,
                .loadOp = loadOp,
                .storeOp = storeOp,
                .clearValue = {.color = clearColor},
            };
        } else {
            mColorAttachmentInfos[i] = {};
        }
    }
    result.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    result.pColorAttachments = mColorAttachmentInfos.data();

    if (depthAttachment && config.enableDepthAttachment) {
        mDepthAttachmentInfo = {
            .imageView = depthAttachment.view,
            .imageLayout = vk::ImageLayout::eAttachmentOptimal,
            .resolveMode = {},
            .resolveImageView = {},
            .resolveImageLayout = {},
            .loadOp = config.depthLoadOp,
            .storeOp = config.depthStoreOp,
            .clearValue = {.depthStencil = {config.clearDepth, config.clearStencil}},
        };
        result.pDepthAttachment = &mDepthAttachmentInfo;
    }

    if (stencilAttachment && config.enableDepthAttachment) {
        mStencilAttachmentInfo = {
            .imageView = stencilAttachment.view,
            .imageLayout = vk::ImageLayout::eAttachmentOptimal,
            .resolveMode = {},
            .resolveImageView = {},
            .resolveImageLayout = {},
            .loadOp = config.stencilLoadOp,
            .storeOp = config.stencilStoreOp,
            .clearValue = {.depthStencil = {config.clearDepth, config.clearStencil}},
        };
        result.pStencilAttachment = &mStencilAttachmentInfo;
    }

    return result;
}
