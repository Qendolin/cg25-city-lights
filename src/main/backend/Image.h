#pragma once
#include <filesystem>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "ImageResource.h"


/// <summary>
/// A container for raw, plain image data on the CPU.
/// This class can either own the data or just hold a view of it.
/// </summary>
class PlainImageData {
    unsigned char *mData = nullptr;
    bool mOwning = false;

public:
    /// <summary>The width of the image in pixels.</summary>
    uint32_t width = 0;
    /// <summary>The height of the image in pixels.</summary>
    uint32_t height = 0;
    /// <summary>The number of color channels of the image.</summary>
    uint32_t channels = 0;
    /// <summary>A span of the raw pixel data.</summary>
    std::span<unsigned char> pixels = {};
    /// <summary>The Vulkan format of the pixel data.</summary>
    vk::Format format = vk::Format::eUndefined;

    /// <summary>
    /// Creates an empty PlainImageData object.
    /// </summary>
    PlainImageData() noexcept;
    ~PlainImageData() noexcept;

    /// <summary>
    /// Creates a PlainImageData object from existing pixel data without taking ownership.
    /// </summary>
    /// <param name="pixels">The raw pixel data.</param>
    /// <param name="width">The width of the image.</param>
    /// <param name="height">The height of the image.</param>
    /// <param name="channels">The number of color channels of the image.</param>
    /// <param name="format">The Vulkan format of the pixel data.</param>
    PlainImageData(std::span<unsigned char> pixels, uint32_t width, uint32_t height, uint32_t channels, vk::Format format);

    /// <summary>
    /// Creates a PlainImageData object that takes ownership of the provided data.
    /// </summary>
    /// <param name="data">A unique_ptr to the raw pixel data.</param>
    /// <param name="size">The size of the data in bytes.</param>
    /// <param name="width">The width of the image.</param>
    /// <param name="height">The height of the image.</param>
    /// <param name="channels">The number of color channels of the image.</param>
    /// <param name="format">The Vulkan format of the pixel data.</param>
    PlainImageData(std::unique_ptr<unsigned char> data, size_t size, uint32_t width, uint32_t height, uint32_t channels, vk::Format format);

    PlainImageData(PlainImageData &&other) noexcept;
    PlainImageData &operator=(PlainImageData &&other) noexcept;

    PlainImageData(const PlainImageData &other) noexcept;
    PlainImageData &operator=(const PlainImageData &other) noexcept;

    /// <summary>
    /// Checks if the image data is valid (i.e., not null).
    /// </summary>
    /// <returns>True if the data is valid, false otherwise.</returns>
    explicit operator bool() const { return static_cast<bool>(mData); }

    /// <summary>
    /// Copies channels from this image to a destination image based on a mapping.
    /// For example, mapping {0, 1, 2} would copy R, G, B channels.
    /// </summary>
    /// <param name="dst">The destination image data.</param>
    /// <param name="mapping">An initializer list specifying the channel mapping.</param>
    void copyChannels(PlainImageData &dst, std::initializer_list<int> mapping) const;

    /// <summary>
    /// Fills specified channels of the image with given values.
    /// </summary>
    /// <param name="channel_list">The channels to fill.</param>
    /// <param name="values">The values to fill the channels with.</param>
    void fill(std::initializer_list<int> channel_list, std::initializer_list<unsigned char> values);

    /// <summary>
    /// Creates a PlainImageData object by loading an image from a file.
    /// </summary>
    /// <param name="format">The desired Vulkan format.</param>
    /// <param name="path">The path to the image file.</param>
    /// <returns>A new PlainImageData object.</returns>
    static PlainImageData create(vk::Format format, const std::filesystem::path &path);

    /// <summary>
    /// Creates a PlainImageData object from raw data.
    /// </summary>
    /// <param name="format">The Vulkan format of the data.</param>
    /// <param name="width">The width of the image.</param>
    /// <param name="height">The height of the image.</param>
    /// <param name="channels">The number of channels in the source data. If 0, it's deduced from format.</param>
    /// <param name="data">The raw pixel data.</param>
    /// <returns>A new PlainImageData object.</returns>
    static PlainImageData create(
            vk::Format format, uint32_t width, uint32_t height, uint32_t channels = 0, const unsigned char *data = nullptr
    );

    /// <summary>
    /// Creates a PlainImageData object from raw data, converting the number of channels.
    /// </summary>
    /// <param name="width">The width of the image.</param>
    /// <param name="height">The height of the image.</param>
    /// <param name="channels">The desired number of channels.</param>
    /// <param name="src_channels">The number of channels in the source data.</param>
    /// <param name="data">The raw pixel data.</param>
    /// <returns>A new PlainImageData object.</returns>
    static PlainImageData create(
            uint32_t width, uint32_t height, uint32_t channels, uint32_t src_channels, const unsigned char *data = nullptr
    );
};

/// <summary>
/// A struct holding the creation parameters for a Vulkan image.
/// </summary>
struct ImageCreateInfo {
    /// <summary>The Vulkan format of the image.</summary>
    vk::Format format = vk::Format::eUndefined;
    /// <summary>Specifies the intended usage of the image.</summary>
    vk::ImageUsageFlags usage = {};
    /// <summary>The type of the image (e.g., 1D, 2D, 3D).</summary>
    vk::ImageType type = vk::ImageType::e2D;
    /// <summary>The width of the image.</summary>
    uint32_t width = 1;
    /// <summary>The height of the image.</summary>
    uint32_t height = 1;
    /// <summary>The depth of the image (for 3D images).</summary>
    uint32_t depth = 1;
    /// <summary>The number of mipmap levels. UINT32_MAX means all possible levels.</summary>
    uint32_t mip_levels = UINT32_MAX;
    /// <summary>The number of layers in the image array.</summary>
    uint32_t array_layers = 1;

    /// <summary>
    /// Creates an ImageCreateInfo struct from a PlainImageData object.
    /// </summary>
    /// <param name="plain_image_data">The plain image data.</param>
    /// <returns>A corresponding ImageCreateInfo struct.</returns>
    static constexpr ImageCreateInfo from(const PlainImageData &plain_image_data) {
        return {
            .format = plain_image_data.format,
            .width = plain_image_data.width,
            .height = plain_image_data.height,
        };
    }
};

/// <summary>
/// Represents a GPU texture image, which is a wrapper around a Vulkan image and its memory allocation.
/// This class is a move-only type.
/// </summary>
class Image : ImageResource {
    [[nodiscard]] vk::Image getImage() const { return *mImage; }

    [[nodiscard]] vk::ImageSubresourceRange getResourceRange() const {
        return {
            .aspectMask = imageAspectFlags(),
            .levelCount = info.mip_levels,
            .layerCount = info.array_layers,
        };
    }

public:
    ImageCreateInfo info;
    /// <summary>The raw Vulkan image handle. Use with caution.</summary>
    vk::Image image;

    /// <summary>
    /// Creates an empty, invalid Image object.
    /// </summary>
    Image() = default;

    /// <summary>
    /// Constructs an Image from an existing Vulkan image and allocation.
    /// </summary>
    /// <param name="image">A VMA unique image handle.</param>
    /// <param name="allocation">A VMA unique allocation handle.</param>
    /// <param name="create_info">The creation info for the image.</param>
    Image(vma::UniqueImage &&image, vma::UniqueAllocation &&allocation, const ImageCreateInfo &create_info);

    Image(const Image &other) = delete;
    Image &operator=(const Image &other) = delete;

    Image(Image &&other) noexcept = default;
    Image &operator=(Image &&other) noexcept = default;

    /// <summary>
    /// Creates a new image.
    /// </summary>
    /// <param name="allocator">The VMA allocator.</param>
    /// <param name="create_info">The creation info for the image.</param>
    /// <returns>A new Image object.</returns>
    static Image create(const vma::Allocator &allocator, ImageCreateInfo create_info);

    /// <summary>
    /// Loads data into a specific mipmap level of the image from a buffer.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the copy command to.</param>
    /// <param name="level">The mipmap level to load data into.</param>
    /// <param name="region">The region of the image to update.</param>
    /// <param name="data">The buffer containing the data to load.</param>
    void load(const vk::CommandBuffer &cmd_buf, uint32_t level, vk::Extent3D region, const vk::Buffer &data);

    /// <summary>
    /// Generates mipmaps for the image. The image must have been created with all mip levels.
    /// The image must be in a transfer-friendly layout.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the blit commands to.</param>
    void generateMipmaps(const vk::CommandBuffer &cmd_buf);

    /// <summary>
    /// Creates a default image view for this image.
    /// The view covers all mip levels and array layers.
    /// </summary>
    /// <param name="device">The Vulkan logical device.</param>
    /// <returns>A unique handle to the created image view.</returns>
    vk::UniqueImageView createDefaultView(const vk::Device &device);

    /// <summary>
    /// Inserts an image memory barrier for this image.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier into.</param>
    /// <param name="begin">The resource access state before the barrier.</param>
    /// <param name="end">The resource access state after the barrier.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end);

    /// <summary>
    /// Inserts an image memory barrier, transitioning the image to a single state.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier into.</param>
    /// <param name="single">The resource access state to transition to.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single);

    /// <summary>
    /// Transfers ownership of the image between queue families.
    /// It does NOT perform any memory barriers or layout transitions. Execution ordering must be handled with a semaphore.
    /// </summary>
    /// <param name="src_cmd_buf">The command buffer in the source queue to record the barrier into.</param>
    /// <param name="dst_cmd_buf">The command buffer in the destination queue to record the barrier into.</param>
    /// <param name="src_queue">The index of the source queue family.</param>
    /// <param name="dst_queue">The index of the destination queue family.</param>
    void transfer(vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue);

private:
    vma::UniqueImage mImage;
    vma::UniqueAllocation mAllocation;

    [[nodiscard]] vk::ImageAspectFlags imageAspectFlags() const;
};
