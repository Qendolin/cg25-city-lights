#include "Framebuffer.h"

#include <vulkan/utility/vk_format_utils.h>

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

vk::RenderingInfo Framebuffer::renderingInfo(const FramebufferRenderingConfig &config) const {
    vk::RenderingInfo result = {
        .flags = config.flags,
        .renderArea = mArea,
        .layerCount = config.layerCount,
        .viewMask = config.viewMask,
    };

    if (config.enableColorAttachments) {
        for (size_t i = 0; i < colorAttachments.size(); i++) {
            const auto &attachment = colorAttachments[i];
            auto clearColor = i < config.clearColors.size() ? config.clearColors[i] : vk::ClearColorValue{};
            auto loadOp = i < config.colorLoadOps.size() ? config.colorLoadOps[i] : vk::AttachmentLoadOp::eLoad;
            auto storeOp = i < config.colorStoreOps.size() ? config.colorStoreOps[i] : vk::AttachmentStoreOp::eStore;
            if (attachment) {
                mColorAttachmentInfos[i] = {
                    .imageView = attachment.view(),
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
    }

    if (depthAttachment && config.enableDepthAttachment) {
        mDepthAttachmentInfo = {
            .imageView = depthAttachment.view(),
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
            .imageView = stencilAttachment.view(),
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
