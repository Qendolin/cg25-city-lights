#include "Image.h"

#include <cmath>
#include <filesystem>
#include <ranges>
#include <stb_image.h>
#include <utility>
#include <vulkan/utility/vk_format_utils.h>

#include "../util/Logger.h"

template<int SrcCh, int DstCh>
    requires(SrcCh >= 1) && (SrcCh <= 4) && (DstCh >= 1) && (DstCh <= 4)
static void copy_pixels(const unsigned char *src, unsigned char *dst, size_t elements) {
    // clang-format off
    for (size_t i = 0; i < elements; i++) {
        // always copy first
        dst[i * DstCh] = src[i * SrcCh];
        if constexpr (DstCh < SrcCh) {
            if constexpr (DstCh > 1) dst[i * DstCh + 1] = src[i * SrcCh + 1];
            if constexpr (DstCh > 2) dst[i * DstCh + 2] = src[i * SrcCh + 2];
            if constexpr (DstCh > 3) dst[i * DstCh + 3] = src[i * SrcCh + 3];
        } else {
            if constexpr (SrcCh > 1) dst[i * DstCh + 1] = src[i * SrcCh + 1];
            if constexpr (SrcCh > 2) dst[i * DstCh + 2] = src[i * SrcCh + 2];
            if constexpr (SrcCh > 3) dst[i * DstCh + 3] = src[i * SrcCh + 3];

            // extend
            if constexpr (DstCh > SrcCh) {
                if constexpr (DstCh - SrcCh >= 1) dst[i * DstCh + SrcCh] = 0;
                if constexpr (DstCh - SrcCh >= 2) dst[i * DstCh + SrcCh + 1] = 0;
                if constexpr (DstCh - SrcCh >= 3) dst[i * DstCh + SrcCh + 2] = 0;
                if constexpr (DstCh == 4) dst[i * DstCh + 3] = 255;
            }
        }
    }
    // clang-format on
}

static void copy_pixels(const unsigned char *src, uint32_t src_channels, unsigned char *dst, uint32_t dst_channels, size_t elements) {
    constexpr std::array jmp = {
        &copy_pixels<1, 1>, &copy_pixels<2, 1>, &copy_pixels<3, 1>, &copy_pixels<4, 1>,
        &copy_pixels<1, 2>, &copy_pixels<2, 2>, &copy_pixels<3, 2>, &copy_pixels<4, 2>,
        &copy_pixels<1, 3>, &copy_pixels<2, 3>, &copy_pixels<3, 3>, &copy_pixels<4, 3>,
        &copy_pixels<1, 4>, &copy_pixels<2, 4>, &copy_pixels<3, 4>, &copy_pixels<4, 4>,
    };
    const uint32_t index = (src_channels - 1) + 4 * (dst_channels - 1);
    jmp[index](src, dst, elements);
}

PlainImageData::PlainImageData() noexcept = default;

PlainImageData::~PlainImageData() noexcept {
    if (!mOwning)
        return;
    std::free(std::exchange(mData, nullptr));
}

PlainImageData::PlainImageData(std::span<unsigned char> pixels, uint32_t width, uint32_t height, uint32_t channels, vk::Format format)
    : mData(pixels.data()), width(width), height(height), channels(channels), pixels(pixels), format(format) {
    Logger::check(channels > 0, "Channel count must be greater than zero");
}

PlainImageData::PlainImageData(
        std::unique_ptr<unsigned char> data, size_t size, uint32_t width, uint32_t height, uint32_t channels, vk::Format format
)
    : mData(data.release()),
      mOwning(true),
      width(width),
      height(height),
      channels(channels),
      pixels(std::span{this->mData, size}),
      format(format) {
    Logger::check(channels > 0, "Channel count must be greater than zero");
}


PlainImageData::PlainImageData(PlainImageData &&other) noexcept
    : mData(std::exchange(other.mData, nullptr)),
      mOwning(std::exchange(other.mOwning, false)),
      width(std::exchange(other.width, 0)),
      height(std::exchange(other.height, 0)),
      channels(std::exchange(other.channels, 0)),
      pixels(std::exchange(other.pixels, {})),
      format(std::exchange(other.format, vk::Format::eUndefined)) {}

PlainImageData &PlainImageData::operator=(PlainImageData &&other) noexcept {
    if (this == &other)
        return *this;

    if (mOwning)
        std::free(std::exchange(mData, nullptr));
    mData = std::exchange(other.mData, nullptr);
    mOwning = std::exchange(other.mOwning, false);
    width = std::exchange(other.width, 0);
    height = std::exchange(other.height, 0);
    channels = std::exchange(other.channels, 0);
    pixels = std::exchange(other.pixels, {});
    format = std::exchange(other.format, vk::Format::eUndefined);
    return *this;
}

PlainImageData::PlainImageData(const PlainImageData &other) noexcept
    : mData(other.mData),
      mOwning(false),
      width(other.width),
      height(other.height),
      channels(other.channels),
      pixels(other.pixels),
      format(other.format) {}

PlainImageData &PlainImageData::operator=(const PlainImageData &other) noexcept {
    if (this == &other)
        return *this;
    mData = other.mData;
    mOwning = false;
    width = other.width;
    height = other.height;
    channels = other.channels;
    pixels = other.pixels;
    format = other.format;
    return *this;
}

PlainImageData PlainImageData::create(
        vk::Format format, uint32_t width, uint32_t height, uint32_t src_channels, const unsigned char *src_data
) {
    uint32_t dst_channels = src_channels;
    if (format != vk::Format::eUndefined)
        dst_channels = vkuFormatComponentCount(static_cast<VkFormat>(format));

    size_t elements = width * height;
    size_t size = elements * dst_channels;
    auto dst_data = static_cast<unsigned char *>(std::malloc(size));
    if (src_data) {
        copy_pixels(src_data, src_channels, dst_data, dst_channels, elements);
    }

    return {std::unique_ptr<unsigned char>(dst_data), size, width, height, dst_channels, format};
}

PlainImageData PlainImageData::create(
        uint32_t width, uint32_t height, uint32_t channels, uint32_t src_channels, const unsigned char *src_data
) {
    size_t elements = width * height;
    size_t size = elements * channels;
    auto dst_data = static_cast<unsigned char *>(std::malloc(size));
    if (src_data) {
        copy_pixels(src_data, src_channels, dst_data, channels, elements);
    }

    return {std::unique_ptr<unsigned char>(dst_data), size, width, height, channels, vk::Format::eUndefined};
}

void PlainImageData::copyChannels(PlainImageData &dst, std::initializer_list<int> mapping) const {
    if (dst.width != width || dst.height != height) {
        Logger::fatal("Texture dimensions do not match");
    }

    auto channel_map = std::span(mapping);
    if (channel_map.size() > channels) {
        Logger::fatal("Too many channels specified");
    }

    const auto copy_channels = std::min(channels, static_cast<uint32_t>(channel_map.size()));
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            for (uint32_t sc = 0; sc < copy_channels; ++sc) {
                int dc = channel_map[sc];
                if (dc < 0)
                    continue;

                size_t i = x + width * y;
                size_t si = i * channels + sc;
                size_t di = i * dst.channels + dc;
                dst.pixels[di] = pixels[si];
            }
        }
    }
}

void PlainImageData::fill(std::initializer_list<int> channel_list, std::initializer_list<unsigned char> values) {
    auto channels_span = std::span(channel_list);
    auto values_span = std::span(values);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            for (uint32_t c = 0; c < channels_span.size(); ++c) {
                int sc = channels_span[c];
                size_t i = x + width * y;
                size_t si = i * channels + sc;
                unsigned char value = values_span[c];
                pixels[si] = value;
            }
        }
    }
}

PlainImageData PlainImageData::create(vk::Format format, const std::filesystem::path &path) {
    int result_channels = static_cast<int>(vkuFormatComponentCount(static_cast<VkFormat>(format)));
    int width, height, channels;
    stbi_uc *pixels = stbi_load(path.string().c_str(), &width, &height, &channels, result_channels);

    // clang-format off
    return {
        std::unique_ptr<unsigned char>(pixels),
        static_cast<size_t>(width * height * result_channels),
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height),
        static_cast<uint32_t>(result_channels),
        format,
    };
    // clang-format on
}

Image::Image(vma::UniqueImage &&image, vma::UniqueAllocation &&allocation, const ImageCreateInfo &create_info)
    : info(create_info), image(*image), mImage(std::move(image)), mAllocation(std::move(allocation)) {}

Image Image::create(const vma::Allocator &allocator, ImageCreateInfo create_info) {
    if (create_info.mip_levels == UINT32_MAX) {
        create_info.mip_levels =
                static_cast<uint32_t>(std::floor(std::log2(std::max(create_info.width, create_info.height)))) + 1;
    }

    auto [image, allocation] = allocator.createImageUnique(
            vk::ImageCreateInfo{
                .imageType = create_info.type,
                .format = create_info.format,
                .extent = {.width = create_info.width, .height = create_info.height, .depth = create_info.depth},
                .mipLevels = create_info.mip_levels,
                .arrayLayers = create_info.array_layers,
                .usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | create_info.usage,
            },
            {
                .usage = vma::MemoryUsage::eAuto,
                .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
            }
    );

    return {std::move(image), std::move(allocation), create_info};
}

void Image::load(const vk::CommandBuffer &cmd_buf, uint32_t level, vk::Extent3D region, const vk::Buffer &data) {
    if (region.width == 0)
        region.width = info.width;
    if (region.height == 0)
        region.height = info.height;
    if (region.depth == 0)
        region.depth = info.depth;

    barrier(cmd_buf, ImageResourceAccess::TransferWrite);

    vk::BufferImageCopy image_copy = {
        .imageSubresource = {.aspectMask = imageAspectFlags(), .mipLevel = level, .layerCount = 1},
        .imageExtent = region,
    };
    cmd_buf.copyBufferToImage(data, *mImage, vk::ImageLayout::eTransferDstOptimal, image_copy);
}

void Image::generateMipmaps(const vk::CommandBuffer &cmd_buf) {
    barrier(cmd_buf, ImageResourceAccess::TransferWrite);

    vk::ImageMemoryBarrier2 barrier = {
        .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
        .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
        .image = *mImage,
        .subresourceRange =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = info.array_layers,
                },
    };

    auto level_width = static_cast<int32_t>(info.width);
    auto level_height = static_cast<int32_t>(info.height);

    // run for images 1..n, the 0th is expected to be loaded
    for (uint32_t lvl = 1; lvl < info.mip_levels; lvl++) {
        int32_t next_level_width = std::max(level_width / 2, 1);
        int32_t next_level_height = std::max(level_height / 2, 1);

        // transition layout of lower mip to src
        if (mPrevAccess.layout != vk::ImageLayout::eTransferSrcOptimal) {
            barrier.subresourceRange.baseMipLevel = lvl - 1;
            barrier.oldLayout = mPrevAccess.layout;
            barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
            barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
            barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
            barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;

            cmd_buf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
        }

        vk::ImageBlit blit = {
            .srcSubresource =
                    {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = lvl - 1,
                        .baseArrayLayer = 0,
                        .layerCount = info.array_layers,
                    },
            .srcOffsets = std::array{vk::Offset3D{0, 0, 0}, vk::Offset3D{level_width, level_height, 1}},
            .dstSubresource =
                    {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .mipLevel = lvl,
                        .baseArrayLayer = 0,
                        .layerCount = info.array_layers,
                    },
            .dstOffsets = std::array{vk::Offset3D{0, 0, 0}, vk::Offset3D{next_level_width, next_level_height, 1}}
        };

        cmd_buf.blitImage(
                *mImage, vk::ImageLayout::eTransferSrcOptimal, *mImage, vk::ImageLayout::eTransferDstOptimal, blit,
                vk::Filter::eLinear
        );

        level_width = next_level_width;
        level_height = next_level_height;
    }

    // final transition, kinda useless, but brings all levels to the same layout
    if (mPrevAccess.layout != vk::ImageLayout::eTransferSrcOptimal) {
        barrier.subresourceRange.baseMipLevel = info.mip_levels - 1;
        barrier.oldLayout = mPrevAccess.layout;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
        barrier.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
        barrier.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;

        cmd_buf.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
    }

    mPrevAccess = {
        .stage = vk::PipelineStageFlagBits2::eTransfer,
        .access = vk::AccessFlagBits2::eTransferRead,
        .layout = vk::ImageLayout::eTransferSrcOptimal
    };
}

vk::UniqueImageView Image::createDefaultView(const vk::Device &device) {
    return device.createImageViewUnique({
        .image = *mImage,
        .viewType = static_cast<vk::ImageViewType>(info.type),
        .format = info.format,
        .subresourceRange = {.aspectMask = imageAspectFlags(), .levelCount = info.mip_levels, .layerCount = info.array_layers},
    });
}

void Image::barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end) {
    ImageResource::barrier(
            *mImage, {.aspectMask = imageAspectFlags(), .levelCount = info.mip_levels, .layerCount = info.array_layers},
            cmd_buf, begin, end
    );
}

void Image::barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single) {
    barrier(cmd_buf, single, single);
}

void Image::transfer(vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue) {
    vk::ImageSubresourceRange range = {
        .aspectMask = imageAspectFlags(), .levelCount = info.mip_levels, .layerCount = info.array_layers
    };

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

vk::ImageAspectFlags Image::imageAspectFlags() const {
    switch (info.format) {
        case vk::Format::eUndefined:
            Logger::fatal("image format undefined");
        case vk::Format::eS8Uint:
            return vk::ImageAspectFlagBits::eStencil;
        case vk::Format::eD16Unorm:
        case vk::Format::eD32Sfloat:
        case vk::Format::eX8D24UnormPack32:
            return vk::ImageAspectFlagBits::eDepth;
        case vk::Format::eD16UnormS8Uint:
        case vk::Format::eD24UnormS8Uint:
        case vk::Format::eD32SfloatS8Uint:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        default:
            if (info.format > vk::Format::eAstc12x12SrgbBlock)
                Logger::fatal("unsupported image format");
            return vk::ImageAspectFlagBits::eColor;
    }
}
