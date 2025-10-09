#include "Image.h"


constexpr ImageResourceAccess ImageResourceAccess::TransferWrite = {
    .stage = vk::PipelineStageFlagBits2::eTransfer,
    .access = vk::AccessFlagBits2::eTransferWrite,
    .layout = vk::ImageLayout::eTransferDstOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::ComputeShaderWriteGeneral = {
    .stage = vk::PipelineStageFlagBits2::eComputeShader,
    .access = vk::AccessFlagBits2::eShaderWrite,
    .layout = vk::ImageLayout::eGeneral
};

constexpr ImageResourceAccess ImageResourceAccess::ComputeShaderReadGeneral = {
    .stage = vk::PipelineStageFlagBits2::eComputeShader,
    .access = vk::AccessFlagBits2::eShaderRead,
    .layout = vk::ImageLayout::eGeneral
};

constexpr ImageResourceAccess ImageResourceAccess::ComputeShaderReadOptimal = {
    .stage = vk::PipelineStageFlagBits2::eComputeShader,
    .access = vk::AccessFlagBits2::eShaderRead,
    .layout = vk::ImageLayout::eShaderReadOnlyOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::FragmentShaderRead = {
    .stage = vk::PipelineStageFlagBits2::eFragmentShader,
    .access = vk::AccessFlagBits2::eShaderRead,
    .layout = vk::ImageLayout::eTransferDstOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::ColorAttachmentWrite = {
    .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    .access = vk::AccessFlagBits2::eColorAttachmentWrite,
    .layout = vk::ImageLayout::eAttachmentOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::DepthAttachmentRead = {
    .stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
    .access = vk::AccessFlagBits2::eDepthStencilAttachmentRead,
    .layout = vk::ImageLayout::eAttachmentOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::DepthAttachmentWrite = {
    .stage = vk::PipelineStageFlagBits2::eLateFragmentTests,
    .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    .layout = vk::ImageLayout::eAttachmentOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::PresentSrc = {
    .stage = vk::PipelineStageFlagBits2::eBottomOfPipe, .access = vk::AccessFlagBits2::eNone, .layout = vk::ImageLayout::ePresentSrcKHR
};


void ImageResource::barrier(
        vk::Image image,
        vk::ImageSubresourceRange range,
        const vk::CommandBuffer &cmd_buf,
        const ImageResourceAccess &begin,
        const ImageResourceAccess &end
) {
    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = mPrevAccess.stage,
        .srcAccessMask = mPrevAccess.access,
        .dstStageMask = begin.stage,
        .dstAccessMask = begin.access,
        .oldLayout = mPrevAccess.layout,
        .newLayout = begin.layout,
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = image,
        .subresourceRange = range,
    };

    cmd_buf.pipelineBarrier2({
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    });
    mPrevAccess = end;
}


void Attachment::barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end) {
    ImageResource::barrier(image, range, cmd_buf, begin, end);
}

void Attachment::barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single) {
    barrier(cmd_buf, single, single);
}

vk::RenderingInfo Framebuffer::renderingInfo(const vk::Rect2D &area, const FramebufferRenderingConfig &config) {
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