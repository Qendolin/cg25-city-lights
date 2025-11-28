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


/// <summary>
/// Defines the physical properties (dimensions, format, layers) of a Vulkan image resource.
/// </summary>
struct ImageInfo {
    vk::Format format = vk::Format::eUndefined;
    vk::ImageAspectFlags aspects = {};
    vk::ImageType type = vk::ImageType::e2D;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    uint32_t levels = 1;
    uint32_t layers = 1;

    static vk::ImageAspectFlags getAspectsFromFormat(const vk::Format &format);
    static vk::ImageUsageFlags getAttachmentUsageFromFormat(const vk::Format &format);

    [[nodiscard]] constexpr vk::ImageSubresourceRange resourceRange() const {
        return {.aspectMask = aspects, .levelCount = levels, .layerCount = layers};
    }

    [[nodiscard]] constexpr vk::Extent3D extents() const { return {width, height, depth}; }
};

/// <summary>
/// Defines how a physical image should be interpreted by shaders (subresource range, view type).
/// </summary>
struct ImageViewInfo {
    vk::Format format = vk::Format::eUndefined;
    vk::ImageViewType type = vk::ImageViewType::e2D;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;
    vk::ImageSubresourceRange resourceRange = {};

    static ImageViewInfo from(const ImageInfo &info);

    [[nodiscard]] constexpr vk::Extent3D extents() const { return {width, height, depth}; }
};


/// <summary>
/// Configuration for creating a new image via VMA.
/// Includes memory usage flags and automatic mip-level calculation settings.
/// </summary>
struct ImageCreateInfo {
    vk::Format format = vk::Format::eUndefined;
    vk::ImageAspectFlags aspects = {};
    vk::ImageType type = vk::ImageType::e2D;
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t depth = 1;

    /// <summary>Set to UINT32_MAX to automatically calculate the maximum number of mip levels based on extents.</summary>
    uint32_t levels = 1;
    uint32_t layers = 1;

    vk::ImageUsageFlags usage = {};
    vk::ImageCreateFlags flags = {};
    vma::MemoryUsage device = vma::MemoryUsage::eAuto;
    vk::MemoryPropertyFlags requiredProperties = vk::MemoryPropertyFlagBits::eDeviceLocal;
    vk::MemoryPropertyFlags preferredProperties = {};

    constexpr operator ImageInfo() const { // NOLINT(*-explicit-constructor)
        return {format, aspects, type, width, height, depth, levels, layers};
    }

    static vk::ImageAspectFlags getAspectsFromFormat(const vk::Format &format) {
        return ImageInfo::getAspectsFromFormat(format);
    }
    static vk::ImageUsageFlags getAttachmentUsageFromFormat(const vk::Format &format) {
        return ImageInfo::getAttachmentUsageFromFormat(format);
    }

    [[nodiscard]] constexpr vk::ImageSubresourceRange resourceRange() const {
        return {.aspectMask = aspects, .baseMipLevel = 0, .levelCount = levels, .baseArrayLayer = 0, .layerCount = layers};
    }

    [[nodiscard]] constexpr vk::Extent3D extents() const { return {width, height, depth}; }
};

/// <summary>
/// Logic layer for Image operations.
/// Provides methods for barriers, layout transitions, and staging copies regardless of ownership or memory backing.
/// </summary>
struct ImageBase : protected ImageResource {
    ImageInfo info = {};

    ImageBase() = default;
    explicit ImageBase(const ImageInfo &info) : info(info) {} // NOLINT(*-explicit-constructor)

    /// <summary>
    /// Records a pipeline barrier for layout transitions and synchronization.
    /// </summary>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &begin, const ImageResourceAccess &end) const {
        ImageResource::barrier(vk::Image(*this), getResourceRange(), cmd_buf, begin, end);
    }

    /// <summary>
    /// Records a barrier where the previous and next access states are identical (e.g. for WAR hazards).
    /// </summary>
    void barrier(const vk::CommandBuffer &cmd_buf, const ImageResourceAccess &single) const {
        barrier(cmd_buf, single, single);
    }

    /// <summary>
    /// Transfers queue family ownership.
    /// <para>Requires a semaphore to synchronize execution order between the source and destination queues.</para>
    /// </summary>
    void transfer(vk::CommandBuffer src_cmd_buf, vk::CommandBuffer dst_cmd_buf, uint32_t src_queue, uint32_t dst_queue) const {
        ImageResource::transfer(vk::Image(*this), getResourceRange(), src_cmd_buf, dst_cmd_buf, src_queue, dst_queue);
    }

    /// <summary>
    /// Copies buffer data into the image. Assumes the image is in a TransferDstOptimal layout.
    /// </summary>
    void load(const vk::CommandBuffer &cmd_buf, uint32_t level, vk::Extent3D region, const vk::Buffer &data);

    /// <summary>
    /// Generates full mipmaps using `vkCmdBlitImage`.
    /// The image layout will be transitioned to `TransferSrcOptimal` for the last mip level upon completion.
    /// </summary>
    void generateMipmaps(const vk::CommandBuffer &cmd_buf);

    [[nodiscard]] vk::ImageSubresourceRange getResourceRange() const {
        return {.aspectMask = info.aspects, .levelCount = info.levels, .layerCount = info.layers};
    }

    virtual operator vk::Image() const = 0; // NOLINT(*-explicit-constructor)
    explicit virtual operator bool() const = 0;
};

/// <summary>
/// Abstract base for any object capable of acting as a Vulkan Image View.
/// </summary>
struct ImageViewBase {
    ImageViewInfo info = {};

    ImageViewBase() = default;
    virtual ~ImageViewBase() = default;

    explicit ImageViewBase(const ImageViewInfo &info) : info(info) {}

    virtual operator vk::ImageView() const = 0; // NOLINT(*-explicit-constructor)
    explicit virtual operator bool() const { return static_cast<vk::ImageView>(*this); }
};


/// <summary>
/// An RAII wrapper that owns a `vk::UniqueImageView`.
/// Use this when you own the lifecycle of the view.
/// </summary>
struct ImageView : ImageViewBase {
    vk::UniqueImageView view = {};

    ImageView() = default;
    ~ImageView() override = default;

    ImageView(vk::UniqueImageView &&view, const ImageViewInfo &info) : ImageViewBase(info), view(std::move(view)) {}

    /// <summary>
    /// Creates a view for the given image resource.
    /// </summary>
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

    operator vk::ImageView() const override { return *view; } // NOLINT(*-explicit-constructor)
};

/// <summary>
/// A lightweight, copyable wrapper for a raw vk::ImageView.
/// Does not own the view or track state.
/// </summary>
struct ImageViewRef : ImageViewBase {
    vk::ImageView view = {};

    ImageViewRef() = default;
    ~ImageViewRef() override = default;

    ImageViewRef(const vk::ImageView &view, const ImageViewInfo &info) : ImageViewBase(info), view(view) {}

    explicit ImageViewRef(const ImageViewBase &view) : ImageViewBase(view), view(view) {}

    operator vk::ImageView() const override { return view; } // NOLINT(*-explicit-constructor)
};


/// <summary>
/// An RAII wrapper representing a dedicated GPU image allocation.
/// Manages the `vma::Allocation` and `vk::Image` lifecycles.
/// </summary>
struct Image : ImageBase {
    vma::UniqueImage image = {};
    vma::UniqueAllocation allocation = {};

    Image() = default;

    Image(vma::UniqueImage &&image, vma::UniqueAllocation &&allocation, const ImageInfo &info)
        : ImageBase(info), image(std::move(image)), allocation(std::move(allocation)) {}

    ~Image() override = default;

    Image(const Image &other) = delete;
    Image &operator=(const Image &other) = delete;

    Image(Image &&other) noexcept = default;
    Image &operator=(Image &&other) noexcept = default;

    operator vk::Image() const override { return *image; } // NOLINT(*-explicit-constructor)
    explicit operator bool() const override { return *image; }

    /// <summary>
    /// Allocates GPU memory and creates a Vulkan image.
    /// <para>
    /// If `create_info.levels` is UINT32_MAX, mip levels are automatically calculated.
    /// Always forces `eTransferSrc | eTransferDst` usage to support `load()` and `generateMipmaps()`.
    /// </para>
    /// </summary>
    static Image create(const vma::Allocator &allocator, const ImageCreateInfo &create_info);
};

/// <summary>
/// A convenience wrapper that owns both an Image and a matching default ImageView.
/// Simplifies the common case of "Creating a Texture" where the image and view lifetimes are identical.
/// </summary>
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

    /// <summary>
    /// Allocates an Image (via VMA) and immediately creates a corresponding ImageView.
    /// <para>
    /// If the level count of `viewCreateInfo` is set to `UINT32_MAX`,
    /// it will be automatically resolved to the image's actual calculated mip level count.
    /// </para>
    /// </summary>
    static ImageWithView create(
            const vk::Device &device,
            const vma::Allocator &allocator,
            const ImageCreateInfo &imageCreateInfo,
            const ImageViewInfo &viewCreateInfo
    );

    static ImageWithView create(const vk::Device &device, const vma::Allocator &allocator, const ImageCreateInfo &createInfo) {
        return create(device, allocator, createInfo, ImageViewInfo::from(createInfo));
    }

    operator TransientImageViewPair() const; // NOLINT(*-explicit-constructor)
    explicit operator bool() const override { return image && view; }
};

/// <summary>
/// Wraps a raw vk::Image to provide barrier tracking logic without owning the memory.
/// <para>
/// This class is Move-Only to ensure the internal barrier state is not duplicated.
/// Use this to import Swapchain images or external resources.
/// </para>
/// </summary>
struct UnmanagedImage : ImageBase {
    vk::Image image;

    UnmanagedImage() = default;

    UnmanagedImage(const vk::Image &image, const ImageInfo &info) : ImageBase(info), image(image) {}

    UnmanagedImage(UnmanagedImage &&other) noexcept;
    UnmanagedImage &operator=(UnmanagedImage &&other) noexcept;

    UnmanagedImage(const UnmanagedImage &other) = delete;
    UnmanagedImage &operator=(const UnmanagedImage &other) = delete;

    /// <summary>
    /// Manually updates the internal barrier state.
    /// Useful when the image layout was modified by an external system (e.g. RenderPass implicit transitions).
    /// </summary>
    void setBarrierState(const ImageResourceAccess &last_access) const { mPrevAccess = last_access; }

    operator vk::Image() const override { return image; } // NOLINT(*-explicit-constructor)
    explicit operator bool() const override { return image; }
};

/// <summary>
/// A non-owning wrapper for both a raw Image and a raw ImageView.
/// Move-Only because the underlying UnmanagedImage tracks state.
/// </summary>
struct UnmanagedImageWithViewRef : UnmanagedImage, ImageViewRef {
    UnmanagedImageWithViewRef() = default;

    UnmanagedImageWithViewRef(const vk::Image &image, const ImageInfo &info, const vk::ImageView &view, const ImageViewInfo &viewInfo)
        : UnmanagedImage(image, info), ImageViewRef(view, viewInfo) {}

    UnmanagedImageWithViewRef(UnmanagedImage &&image, const ImageViewRef &view)
        : UnmanagedImage(std::move(image)), ImageViewRef(view) {}
    UnmanagedImageWithViewRef(UnmanagedImage &&image, const vk::ImageView &view, const ImageViewInfo &info)
        : UnmanagedImage(std::move(image)), ImageViewRef(view, info) {}

    UnmanagedImageWithViewRef(UnmanagedImageWithViewRef &&) noexcept = default;
    UnmanagedImageWithViewRef &operator=(UnmanagedImageWithViewRef &&) noexcept = default;

    operator TransientImageViewPair() const; // NOLINT(*-explicit-constructor)
    explicit operator bool() const override { return image && view; }
};

/// <summary>
/// A hybrid structure: Unmanaged Image (Swapchain) + Owned ImageView (RAII).
/// Common for Swapchains where you receive the image but must create your own views.
/// </summary>
struct UnmanagedImageWithView : UnmanagedImage, ImageView {
    UnmanagedImageWithView() = default;

    UnmanagedImageWithView(const vk::Image &image, const ImageInfo &imageInfo, vk::UniqueImageView &&view, const ImageViewInfo &viewInfo)
        : UnmanagedImage(image, imageInfo), ImageView(std::move(view), viewInfo) {}

    UnmanagedImageWithView(UnmanagedImage &&image, vk::UniqueImageView &&view, const ImageViewInfo &viewInfo)
        : UnmanagedImage(std::move(image)), ImageView(std::move(view), viewInfo) {}

    UnmanagedImageWithView(const UnmanagedImageWithView &other) = delete;
    UnmanagedImageWithView &operator=(const UnmanagedImageWithView &other) = delete;

    UnmanagedImageWithView(UnmanagedImageWithView &&other) noexcept = default;
    UnmanagedImageWithView &operator=(UnmanagedImageWithView &&other) noexcept = default;

    operator TransientImageViewPair() const; // NOLINT(*-explicit-constructor)
    explicit operator bool() const override { return image && view; }
};

struct ImageViewPairBase {
    virtual ~ImageViewPairBase() = default;

    [[nodiscard]] virtual const ImageBase &image() const = 0;
    [[nodiscard]] virtual const ImageViewBase &view() const = 0;

    operator vk::Image() const { return image(); } // NOLINT(*-explicit-constructor)
    operator vk::ImageView() const { return view(); } // NOLINT(*-explicit-constructor)
    operator const ImageBase &() const { return image(); } // NOLINT(*-explicit-constructor)
    operator const ImageViewBase &() const { return view(); } // NOLINT(*-explicit-constructor)
    virtual explicit operator bool() const = 0;
};

/// <summary>
/// A persistent container that points to an existing Image and View instance.
/// Holds pointers, so the referenced objects must outlive this pair.
/// </summary>
class ImageViewPair : public ImageViewPairBase {
    const ImageBase *mImage = nullptr;
    const ImageViewBase *mView = nullptr;

public:
    ImageViewPair() = default;
    explicit ImageViewPair(const ImageWithView &image_with_view) : mImage(&image_with_view), mView(&image_with_view) {}
    explicit ImageViewPair(const UnmanagedImageWithViewRef &image_with_view)
        : mImage(&image_with_view), mView(&image_with_view) {}
    explicit ImageViewPair(const UnmanagedImageWithView &image_with_view)
        : mImage(&image_with_view), mView(&image_with_view) {}
    ImageViewPair(const ImageBase &image, const ImageViewBase &view) : mImage(&image), mView(&view) {}

    // No temporaries
    ImageViewPair(ImageBase &&image, const ImageViewBase &view) = delete;
    ImageViewPair(const ImageBase &image, ImageViewBase &&view) = delete;
    ImageViewPair(ImageBase &&image, ImageViewBase &&view) = delete;

    [[nodiscard]] const ImageBase &image() const override { return *mImage; }
    [[nodiscard]] const ImageViewBase &view() const override { return *mView; }

    explicit operator bool() const override { return mImage && *mImage && mView && *mView; }
};


/// <summary>
/// A lightweight temporary reference to an image and its view.
/// Designed for passing combined resources to functions (e.g. render targets) without transferring ownership.
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

    [[nodiscard]] const ImageBase &image() const override { return mImage; }
    [[nodiscard]] const ImageViewBase &view() const override { return mView; }

    explicit operator bool() const override { return mImage && mView; }
};
