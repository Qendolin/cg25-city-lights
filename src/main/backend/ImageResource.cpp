#include "ImageResource.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>

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

constexpr ImageResourceAccess ImageResourceAccess::FragmentShaderReadOptimal = {
    .stage = vk::PipelineStageFlagBits2::eFragmentShader,
    .access = vk::AccessFlagBits2::eShaderRead,
    .layout = vk::ImageLayout::eReadOnlyOptimal
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
) const {
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
