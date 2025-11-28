#pragma once
#include <filesystem>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "ImageResource.h"


class TransientImageViewPair;

/// <summary>
/// A container for raw, plain image data on the CPU.
/// This class can either own the data or just hold a view of it.
/// </summary>
template<typename T>
class PlainImageData {
    T *mData = nullptr;
    bool mOwning = false;

public:
    /// <summary>The width of the image in pixels.</summary>
    uint32_t width = 0;
    /// <summary>The height of the image in pixels.</summary>
    uint32_t height = 0;
    /// <summary>The number of color channels of the image.</summary>
    uint32_t channels = 0;
    /// <summary>A span of the raw pixel data.</summary>
    std::span<T> pixels = {};
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
    PlainImageData(std::span<T> pixels, uint32_t width, uint32_t height, uint32_t channels, vk::Format format);

    /// <summary>
    /// Creates a PlainImageData object that takes ownership of the provided data.
    /// </summary>
    /// <param name="data">A unique_ptr to the raw pixel data.</param>
    /// <param name="count">The number of elements in the pixel data.</param>
    /// <param name="width">The width of the image.</param>
    /// <param name="height">The height of the image.</param>
    /// <param name="channels">The number of color channels of the image.</param>
    /// <param name="format">The Vulkan format of the pixel data.</param>
    PlainImageData(std::unique_ptr<T> data, size_t count, uint32_t width, uint32_t height, uint32_t channels, vk::Format format);

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
    void fill(std::initializer_list<int> channel_list, std::initializer_list<T> values);

    /// <summary>
    /// Creates a PlainImageData object by loading an image from a file.
    /// </summary>
    /// <param name="format">The desired Vulkan format.</param>
    /// <param name="path">The path to the image file.</param>
    /// <returns>A new PlainImageData object.</returns>
    static PlainImageData create(vk::Format format, const std::filesystem::path &path)
        requires std::is_same_v<T, float> || std::is_same_v<T, uint16_t> || std::is_same_v<T, uint8_t>;

    /// <summary>
    /// Creates a PlainImageData object from raw data.
    /// </summary>
    /// <param name="format">The Vulkan format of the data.</param>
    /// <param name="width">The width of the image.</param>
    /// <param name="height">The height of the image.</param>
    /// <param name="src_channels">The number of channels in the source data. If 0, it's deduced from format.</param>
    /// <param name="src_data">The raw pixel data.</param>
    /// <returns>A new PlainImageData object.</returns>
    static PlainImageData create(
            vk::Format format, uint32_t width, uint32_t height, uint32_t src_channels = 0, const T *src_data = nullptr
    );

    /// <summary>
    /// Creates a PlainImageData object from raw data, converting the number of channels.
    /// </summary>
    /// <param name="width">The width of the image.</param>
    /// <param name="height">The height of the image.</param>
    /// <param name="channels">The desired number of channels.</param>
    /// <param name="src_channels">The number of channels in the source data.</param>
    /// <param name="src_data">The raw pixel data.</param>
    /// <returns>A new PlainImageData object.</returns>
    static PlainImageData create(
            uint32_t width, uint32_t height, uint32_t channels, uint32_t src_channels, const T *src_data = nullptr
    );

private:
    static int getFormatComponentCount(vk::Format format);

    template<int SrcCh, int DstCh>
        requires(SrcCh >= 1) && (SrcCh <= 4) && (DstCh >= 1) && (DstCh <= 4)
    static void copyPixels(const T *src, T *dst, size_t elements);

    static void copyPixels(const T *src, uint32_t src_channels, T *dst, uint32_t dst_channels, size_t elements);
};

using PlainImageDataU8 = PlainImageData<uint8_t>;
using PlainImageDataU16 = PlainImageData<uint16_t>;
using PlainImageDataU32 = PlainImageData<uint32_t>;
using PlainImageDataF = PlainImageData<float>;


struct ImageInfo {
    /// <summary>The Vulkan format of the image.</summary>
    vk::Format format = vk::Format::eUndefined;
    /// <summary>The aspect flags of the image (e.g., color, depth, stencil).</summary>
    vk::ImageAspectFlags aspects = {};
    /// <summary>The type of the image (e.g., 1D, 2D, 3D).</summary>
    vk::ImageType type = vk::ImageType::e2D;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    /// <summary>The number of mipmap levels.</summary>
    uint32_t levels = 1;
    /// <summary>The number of layers in the image array.</summary>
    uint32_t layers = 1;

    static vk::ImageAspectFlags getAspectsFromFormat(const vk::Format &format);
    static vk::ImageUsageFlags getAttachmentUsageFromFormat(const vk::Format &format);

    constexpr [[nodiscard]] vk::ImageSubresourceRange resourceRange() const {
        return {.aspectMask = aspects, .levelCount = levels, .layerCount = layers};
    }

    constexpr [[nodiscard]] vk::Extent3D extents() const { return {width, height, depth}; }
};

struct ImageViewInfo {
    /// <summary>The Vulkan format of the image.</summary>
    vk::Format format = vk::Format::eUndefined;
    /// <summary>The type of the image (e.g., 1D, 2D, 3D).</summary>
    vk::ImageViewType type = vk::ImageViewType::e2D;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    vk::ImageSubresourceRange resourceRange = {};

    static ImageViewInfo from(const ImageInfo &info);

    constexpr [[nodiscard]] vk::Extent3D extents() const { return {width, height, depth}; }
};


/// <summary>
/// A struct holding the creation parameters for a Vulkan image.
/// </summary>
struct ImageCreateInfo {
    /// <summary>The Vulkan format of the image.</summary>
    vk::Format format = vk::Format::eUndefined;
    /// <summary>The aspect flags of the image (e.g., color, depth, stencil).</summary>
    vk::ImageAspectFlags aspects = {};
    /// <summary>The type of the image (e.g., 1D, 2D, 3D).</summary>
    vk::ImageType type = vk::ImageType::e2D;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    /// <summary>The number of mipmap levels.</summary>
    uint32_t levels = 1;
    /// <summary>The number of layers in the image array.</summary>
    uint32_t layers = 1;

    /// <summary>Specifies the intended usage of the image.</summary>
    vk::ImageUsageFlags usage = {};
    /// <summary>Specifies the flags of the image.</summary>
    vk::ImageCreateFlags flags = {};
    vma::MemoryUsage device = vma::MemoryUsage::eAuto;
    vk::MemoryPropertyFlags requiredProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    vk::MemoryPropertyFlags preferredProperties = {};

    constexpr operator ImageInfo() const { return {format, aspects, type, width, height, depth, levels, layers}; }

    static vk::ImageAspectFlags getAspectsFromFormat(const vk::Format &format) {
        return ImageInfo::getAspectsFromFormat(format);
    }
    static vk::ImageUsageFlags getAttachmentUsageFromFormat(const vk::Format &format) {
        return ImageInfo::getAttachmentUsageFromFormat(format);
    }

    constexpr [[nodiscard]] vk::ImageSubresourceRange resourceRange() const {
        return {.aspectMask = aspects, .levelCount = levels, .layerCount = layers};
    }

    constexpr [[nodiscard]] vk::Extent3D extents() const { return {width, height, depth}; }
};

struct ImageBase : protected ImageResource {
    ImageInfo info = {};

    ImageBase() = default;
    ImageBase(const ImageInfo &info) : info(info) {}

    /// <summary>
    /// Inserts an image memory barrier for this image.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier into.</param>
    /// <param name="begin">The resource access state before the barrier.</param>
    /// <param name="end">The resource access state after the barrier.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end) const {
        ImageResource::barrier(vk::Image(*this), getResourceRange(), cmd_buf, begin, end);
    }

    /// <summary>
    /// Inserts an image memory barrier, transitioning the image to a single state.
    /// </summary>
    /// <param name="cmd_buf">The command buffer to record the barrier into.</param>
    /// <param name="single">The resource access state to transition to.</param>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single) const {
        barrier(cmd_buf, single, single);
    }

    /// <summary>
    /// Transfers ownership of the image between queue families.
    /// It does NOT perform any memory barriers or layout transitions. Execution ordering must be handled with a semaphore.
    /// </summary>
    /// <param name="src_cmd_buf">The command buffer in the source queue to record the barrier into.</param>
    /// <param name="dst_cmd_buf">The command buffer in the destination queue to record the barrier into.</param>
    /// <param name="src_queue">The index of the source queue family.</param>
    /// <param name="dst_queue">The index of the destination queue family.</param>
    void transfer(vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue) const {
        ImageResource::transfer(vk::Image(*this), getResourceRange(), src_cmd_buf, dst_cmd_buf, src_queue, dst_queue);
    }

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

    [[nodiscard]] vk::ImageSubresourceRange getResourceRange() const {
        return {.aspectMask = info.aspects, .levelCount = info.levels, .layerCount = info.layers};
    }

    virtual operator vk::Image() const = 0;
    explicit virtual operator bool() const = 0;
};

struct ImageViewBase {
    ImageViewInfo info = {};

    ImageViewBase() = default;
    virtual ~ImageViewBase() = default;

    ImageViewBase(const ImageViewInfo &info) : info(info) {}

    virtual operator vk::ImageView() const = 0;
    explicit virtual operator bool() const { return static_cast<vk::ImageView>(*this); }
};


struct ImageView : ImageViewBase {
    vk::UniqueImageView view = {};

    ImageView() = default;
    ~ImageView() override = default;

    ImageView(vk::UniqueImageView &&view, const ImageViewInfo &info) : ImageViewBase(info), view(std::move(view)) {}

    static ImageView create(const vk::Device &device, const vk::Image &image, const ImageViewInfo &info);

    static ImageView create(const vk::Device &device, const vk::Image &image, const ImageInfo &info) {
        return create(device, image, ImageViewInfo::from(info));
    }

    static ImageView create(const vk::Device &device, const ImageBase &image) {
        return create(device, image, ImageViewInfo::from(image.info));
    }

    ImageView(const ImageView &other) = delete;
    ImageView &operator=(const ImageView &other) = delete;

    ImageView(ImageView &&other) noexcept = default;
    ImageView &operator=(ImageView &&other) noexcept = default;

    operator vk::ImageView() const override { return *view; }
};

struct UnmanagedImageView : ImageViewBase {
    vk::ImageView view = {};

    UnmanagedImageView() = default;
    ~UnmanagedImageView() override = default;

    UnmanagedImageView(const vk::ImageView &view, const ImageViewInfo &info) : ImageViewBase(info), view(view) {}

    explicit UnmanagedImageView(const ImageViewBase &view) : ImageViewBase(view), view(view) {}

    operator vk::ImageView() const override { return view; }
};


/// <summary>
/// Represents a GPU texture image, which is a wrapper around a Vulkan image and its memory allocation.
/// This class is a move-only type.
/// </summary>
struct Image : ImageBase {
    vma::UniqueImage image = {};
    vma::UniqueAllocation allocation = {};

    /// <summary>
    /// Creates an empty, invalid Image object.
    /// </summary>
    Image() = default;

    /// <summary>
    /// Constructs an Image from an existing Vulkan image and allocation.
    /// </summary>
    /// <param name="image">A VMA unique image handle.</param>
    /// <param name="allocation">A VMA unique allocation handle.</param>
    /// <param name="info">The meta info for the image.</param>
    Image(vma::UniqueImage &&image, vma::UniqueAllocation &&allocation, const ImageInfo &info)
        : ImageBase(info), image(std::move(image)), allocation(std::move(allocation)) {}

    ~Image() override = default;

    Image(const Image &other) = delete;
    Image &operator=(const Image &other) = delete;

    Image(Image &&other) noexcept = default;
    Image &operator=(Image &&other) noexcept = default;

    operator vk::Image() const override { return *image; }
    explicit operator bool() const override { return *image; }

    /// <summary>
    /// Creates a new, empty image.
    /// </summary>
    static Image create(const vma::Allocator &allocator, const ImageCreateInfo &create_info);
};

struct ImageWithView : Image, ImageView {
    ImageWithView() = default;

    ImageWithView(
            vma::UniqueImage &&image,
            vma::UniqueAllocation &&allocation,
            const ImageInfo &info,
            vk::UniqueImageView &&view,
            const ImageViewInfo &viewInfo
    )
        : Image(std::move(image), std::move(allocation), info), ImageView(std::move(view), viewInfo) {}

    ImageWithView(Image &&image, vk::UniqueImageView &&view, const ImageViewInfo &viewInfo)
        : Image(std::move(image)), ImageView(std::move(view), viewInfo) {}
    ImageWithView(Image &&image, ImageView &&view) : Image(std::move(image)), ImageView(std::move(view)) {}

    ImageWithView(const ImageWithView &other) = delete;
    ImageWithView &operator=(const ImageWithView &other) = delete;

    ImageWithView(ImageWithView &&other) noexcept = default;
    ImageWithView &operator=(ImageWithView &&other) noexcept = default;

    ~ImageWithView() override = default;

    static ImageWithView create(const vk::Device &device, const vma::Allocator &allocator, const ImageCreateInfo &createInfo) {
        return create(device, allocator, createInfo, ImageViewInfo::from(createInfo));
    }

    static ImageWithView create(
            const vk::Device &device,
            const vma::Allocator &allocator,
            const ImageCreateInfo &imageCreateInfo,
            const ImageViewInfo &viewCreateInfo
    );

    operator TransientImageViewPair() const;
    explicit operator bool() const override { return image && view; }
};

struct UnmanagedImage : ImageBase {
    vk::Image image;

    /// <summary>
    /// Creates an empty, invalid Image object.
    /// </summary>
    UnmanagedImage() = default;

    /// <summary>
    /// Constructs an UnmanagedImage from an existing Vulkan image.
    /// </summary>
    UnmanagedImage(const vk::Image &image, const ImageInfo &info) : ImageBase(info), image(image) {}

    explicit UnmanagedImage(const ImageBase &image) : ImageBase(image), image(image) {}

    void setBarrierState(const ImageResourceAccess &last_access) const { mPrevAccess = last_access; }

    operator vk::Image() const override { return image; }
    explicit operator bool() const override { return image; }
};

struct UnmanagedImageWithView : UnmanagedImage, UnmanagedImageView {
    UnmanagedImageWithView() = default;

    UnmanagedImageWithView(const vk::Image &image, const ImageInfo &info, const vk::ImageView &view, const ImageViewInfo &viewInfo)
        : UnmanagedImage(image, info), UnmanagedImageView(view, viewInfo) {}

    UnmanagedImageWithView(const UnmanagedImage &image, const UnmanagedImageView &view)
        : UnmanagedImage(image), UnmanagedImageView(view) {}
    UnmanagedImageWithView(const UnmanagedImage &image, const vk::ImageView &view, const ImageViewInfo &info)
        : UnmanagedImage(image), UnmanagedImageView(view, info) {}

    operator TransientImageViewPair() const;
    explicit operator bool() const override { return image && view; }
};

struct UnmanagedImageWithManagedView : UnmanagedImage, ImageView {
    UnmanagedImageWithManagedView() = default;

    UnmanagedImageWithManagedView(
            const vk::Image &image, const ImageInfo &imageInfo, vk::UniqueImageView &&view, const ImageViewInfo &viewInfo
    )
        : UnmanagedImage(image, imageInfo), ImageView(std::move(view), viewInfo) {}

    UnmanagedImageWithManagedView(const UnmanagedImage &image, vk::UniqueImageView &&view, const ImageViewInfo &viewInfo)
        : UnmanagedImage(image), ImageView(std::move(view), viewInfo) {}

    UnmanagedImageWithManagedView(const UnmanagedImageWithManagedView &other) = delete;
    UnmanagedImageWithManagedView &operator=(const UnmanagedImageWithManagedView &other) = delete;

    UnmanagedImageWithManagedView(UnmanagedImageWithManagedView &&other) noexcept = default;
    UnmanagedImageWithManagedView &operator=(UnmanagedImageWithManagedView &&other) noexcept = default;

    operator TransientImageViewPair() const;
    explicit operator bool() const override { return image && view; }
};

struct ImageViewPairBase {
    virtual ~ImageViewPairBase() = default;

    virtual const ImageBase &image() const = 0;
    virtual const ImageViewBase &view() const = 0;

    operator vk::Image() const { return image(); }
    operator vk::ImageView() const { return view(); }
    operator const ImageBase &() const { return image(); }
    operator const ImageViewBase &() const { return view(); }
    virtual explicit operator bool() const = 0;
};

class ImageViewPair : public ImageViewPairBase {
    const ImageBase *mImage = nullptr;
    const ImageViewBase *mView = nullptr;

public:
    ImageViewPair() = default;
    explicit ImageViewPair(const ImageWithView &image_with_view) : mImage(&image_with_view), mView(&image_with_view) {}
    explicit ImageViewPair(const UnmanagedImageWithView &image_with_view) : mImage(&image_with_view), mView(&image_with_view) {}
    explicit ImageViewPair(const UnmanagedImageWithManagedView &image_with_view) : mImage(&image_with_view), mView(&image_with_view) {}
    ImageViewPair(const ImageBase &image, const ImageViewBase &view) : mImage(&image), mView(&view) {}

    // No temporaries
    ImageViewPair(ImageBase &&image, const ImageViewBase &view) = delete;
    ImageViewPair(const ImageBase &image, ImageViewBase &&view) = delete;
    ImageViewPair(ImageBase &&image, ImageViewBase &&view) = delete;

    const ImageBase &image() const override { return *mImage; }
    const ImageViewBase &view() const override { return *mView; }

    explicit operator bool() const override {
        return mImage && *mImage && mView && *mView;
    }
};


/// <summary>
/// A temporary reference to an image and its view.
/// </summary>
class TransientImageViewPair : public ImageViewPairBase {
    const ImageBase &mImage;
    const ImageViewBase &mView;

public:
    TransientImageViewPair(const ImageBase &image, const ImageViewBase &view) : mImage(image), mView(view) {}

    TransientImageViewPair(const TransientImageViewPair &) = delete;
    TransientImageViewPair(TransientImageViewPair &&) = delete;
    TransientImageViewPair &operator=(const TransientImageViewPair &) = delete;
    TransientImageViewPair &operator=(TransientImageViewPair &&) = delete;

    const ImageBase &image() const override { return mImage; }
    const ImageViewBase &view() const override { return mView; }

    explicit operator bool() const override {
        return mImage && mView;
    }
};
