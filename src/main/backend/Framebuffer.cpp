#include "Framebuffer.h"


void Attachment::barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end) const {
    ImageResource::barrier(image, range, cmd_buf, begin, end);
}

void Attachment::barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single) const {
    barrier(cmd_buf, single, single);
}

vk::Format Framebuffer::depthFormat() const { return depthAttachment.format; }

vk::RenderingInfo Framebuffer::renderingInfo(const FramebufferRenderingConfig &config) const {
    vk::RenderingInfo result = {
        .flags = config.flags,
        .renderArea = area,
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