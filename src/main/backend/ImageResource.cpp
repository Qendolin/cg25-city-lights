#include "ImageResource.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>

constexpr ImageResourceAccess ImageResourceAccess::TransferWrite = {
    .stage = vk::PipelineStageFlagBits2::eTransfer,
    .access = vk::AccessFlagBits2::eTransferWrite,
    .layout = vk::ImageLayout::eTransferDstOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::TransferRead = {
    .stage = vk::PipelineStageFlagBits2::eTransfer,
    .access = vk::AccessFlagBits2::eTransferRead,
    .layout = vk::ImageLayout::eTransferSrcOptimal
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

constexpr ImageResourceAccess ImageResourceAccess::ColorAttachmentLoad = {
    .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    .access = vk::AccessFlagBits2::eColorAttachmentRead,
    .layout = vk::ImageLayout::eAttachmentOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::ColorAttachmentWrite = {
    .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    // The "load" operation happens in the eColorAttachmentOutput stage and requires read access
    .access = vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead,
    .layout = vk::ImageLayout::eAttachmentOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::DepthAttachmentEarlyOps = {
    // Visibility for the EFT does not imply visibility for the LFT (for example if the EFT is skipped), so it needs to be included explicitly
    .stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
    // Testing involves read and write access. Also the "clear" op also writes during the EFT stage.
    .access = vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    .layout = vk::ImageLayout::eAttachmentOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::DepthAttachmentLateOps = {
    // Depth writes can happen in both the EFT and LFT stage. So for visibility (tho not for execution) both need to be included
    .stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
    // Don't need to include reads, because reads do not need to be made visible (execution ordering is enough).
    .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    .layout = vk::ImageLayout::eAttachmentOptimal
};

constexpr ImageResourceAccess ImageResourceAccess::PresentSrc = {
    .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput, .access = vk::AccessFlagBits2::eMemoryRead, .layout = vk::ImageLayout::ePresentSrcKHR
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


void ImageResource::transfer(
        vk::Image image,
        vk::ImageSubresourceRange range,
        vk::CommandBuffer src_cmd_buf,
        vk::CommandBuffer dst_cmd_buf,
        uint32_t src_queue,
        uint32_t dst_queue
) const {
    vk::ImageMemoryBarrier2 src_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eNone,
        .srcAccessMask = {},
        .dstStageMask = vk::PipelineStageFlagBits2::eNone,
        .dstAccessMask = {},
        .oldLayout = mPrevAccess.layout,
        .newLayout = mPrevAccess.layout,
        .srcQueueFamilyIndex = src_queue,
        .dstQueueFamilyIndex = dst_queue,
        .image = image,
        .subresourceRange = range,
    };

    src_cmd_buf.pipelineBarrier2({
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &src_barrier,
    });
    vk::ImageMemoryBarrier2 dst_barrier{
        .srcStageMask = vk::PipelineStageFlagBits2::eNone,
        .srcAccessMask = {},
        .dstStageMask = vk::PipelineStageFlagBits2::eNone,
        .dstAccessMask = {},
        .oldLayout = mPrevAccess.layout,
        .newLayout = mPrevAccess.layout,
        .srcQueueFamilyIndex = src_queue,
        .dstQueueFamilyIndex = dst_queue,
        .image = image,
        .subresourceRange = range,
    };

    dst_cmd_buf.pipelineBarrier2({
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &dst_barrier,
    });
}
