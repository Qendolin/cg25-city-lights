#pragma once

#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "../glfw/Window.h"

namespace glfw {
    class Window;
}

class WindowContext;

/// <summary>
/// Represents the swapchain, which is a collection of images that are used for rendering and presentation.
/// </summary>
class Swapchain {
public:
    /// <summary>
    /// Initializes a new instance of the Swapchain class.
    /// </summary>
    /// <param name="device">The Vulkan device.</param>
    /// <param name="physical_device">The Vulkan physical device.</param>
    /// <param name="surface">The window surface.</param>
    /// <param name="window">The GLFW window.</param>
    /// <param name="allocator">The VMA allocator.</param>
    Swapchain(const vk::Device& device, const vk::PhysicalDevice& physical_device, const vk::SurfaceKHR& surface, const glfw::Window& window, const vma::Allocator& allocator);

    /// <summary>
    /// Gets the sRGB color format of the swapchain.
    /// </summary>
    /// <returns>The sRGB color format.</returns>
    [[nodiscard]] vk::Format colorFormatSrgb() const { return mSurfaceFormat.format; }

    /// <summary>
    /// Gets the linear color format of the swapchain.
    /// </summary>
    /// <returns>The linear color format, or the sRGB format if a linear format is not available.</returns>
    [[nodiscard]] vk::Format colorFormatLinear() const {
        if (mSurfaceFormatLinear == vk::Format::eUndefined)
            return colorFormatSrgb();
        return mSurfaceFormatLinear;
    }

    /// <summary>
    /// Gets the depth format of the swapchain.
    /// </summary>
    /// <returns>The depth format.</returns>
    [[nodiscard]] vk::Format depthFormat() const { return mDepthFormat; }

    /// <summary>
    /// Gets the number of images in the swapchain.
    /// </summary>
    /// <returns>The image count.</returns>
    [[nodiscard]] int imageCount() const { return mImageCount; }

    /// <summary>
    /// Gets the minimum number of images required for the swapchain.
    /// </summary>
    /// <returns>The minimum image count.</returns>
    [[nodiscard]] int minImageCount() const { return mMinImageCount; }

    /// <summary>
    /// Gets the maximum number of images allowed for the swapchain.
    /// </summary>
    /// <returns>The maximum image count.</returns>
    [[nodiscard]] int maxImageCount() const { return mMaxImageCount; }

    /// <summary>
    /// Gets the index of the currently active image in the swapchain.
    /// </summary>
    /// <returns>The active image index.</returns>
    [[nodiscard]] int activeImageIndex() const { return mActiveImageIndex; }

    /// <summary>
    /// Gets the presentation mode of the swapchain.
    /// </summary>
    /// <returns>The present mode.</returns>
    [[nodiscard]] vk::PresentModeKHR presentMode() const { return mPresentMode; }

    /// <summary>
    /// Gets the extents of the swapchain images.
    /// </summary>
    /// <returns>The surface extents.</returns>
    [[nodiscard]] vk::Extent2D extents() const { return mSurfaceExtents; }

    /// <summary>
    /// Gets the rendering area of the swapchain.
    /// </summary>
    /// <returns>The rendering area.</returns>
    [[nodiscard]] vk::Rect2D area() const { return {{}, mSurfaceExtents}; }

    /// <summary>
    /// Gets the width of the swapchain images.
    /// </summary>
    /// <returns>The width.</returns>
    [[nodiscard]] float width() const { return static_cast<float>(mSurfaceExtents.width); }

    /// <summary>
    /// Gets the height of the swapchain images.
    /// </summary>
    /// <returns>The height.</returns>
    [[nodiscard]] float height() const { return static_cast<float>(mSurfaceExtents.height); }

    /// <summary>
    /// Gets the currently active color image.
    /// </summary>
    /// <returns>The active color image.</returns>
    [[nodiscard]] vk::Image colorImage() const { return mSwapchainImages.at(mActiveImageIndex); }

    /// <summary>
    /// Gets the color image at the specified index.
    /// </summary>
    /// <param name="i">The index of the image.</param>
    /// <returns>The color image.</returns>
    [[nodiscard]] vk::Image colorImage(int i) const { return mSwapchainImages.at(i); }

    /// <summary>
    /// Gets the sRGB image view for the currently active color image.
    /// </summary>
    /// <returns>The active sRGB image view.</returns>
    [[nodiscard]] vk::ImageView colorViewSrgb() const { return *mSwapchainImageViewsSrgb.at(mActiveImageIndex); }

    /// <summary>
    /// Gets the sRGB image view for the color image at the specified index.
    /// </summary>
    /// <param name="i">The index of the image.</param>
    /// <returns>The sRGB image view.</returns>
    [[nodiscard]] vk::ImageView colorViewSrgb(int i) const { return *mSwapchainImageViewsSrgb.at(i); }

    /// <summary>
    /// Gets the linear image view for the currently active color image.
    /// </summary>
    /// <returns>The active linear image view, or the sRGB view if a linear format is not available.</returns>
    [[nodiscard]] vk::ImageView colorViewLinear() const {
        if (mSurfaceFormatLinear == vk::Format::eUndefined)
            return colorViewSrgb();
        return *mSwapchainImageViewsUnorm.at(mActiveImageIndex);
    }

    /// <summary>
    /// Gets the linear image view for the color image at the specified index.
    /// </summary>
    /// <param name="i">The index of the image.</param>
    /// <returns>The linear image view, or the sRGB view if a linear format is not available.</returns>
    [[nodiscard]] vk::ImageView colorViewLinear(int i) const {
        if (mSurfaceFormatLinear == vk::Format::eUndefined)
            return colorViewSrgb(i);
        return *mSwapchainImageViewsUnorm.at(i);
    }

    /// <summary>
    /// Gets the depth image.
    /// </summary>
    /// <returns>The depth image.</returns>
    [[nodiscard]] vk::Image depthImage() const { return *mDepthImage; }

    /// <summary>
    /// Gets the depth image view.
    /// </summary>
    /// <returns>The depth image view.</returns>
    [[nodiscard]] vk::ImageView depthView() const { return *mDepthImageView; }

    /// <summary>
    /// Creates the swapchain.
    /// </summary>
    void create();

    /// <summary>
    /// Recreates the swapchain, typically after a window resize.
    /// </summary>
    void recreate();

    /// <summary>
    /// Marks the swapchain as invalid, forcing a recreate at the next opportunity.
    /// </summary>
    void invalidate() { mInvalid = true; }

    /// <summary>
    /// Acquires the next available image from the swapchain.
    /// </summary>
    /// <param name="image_available_semaphore">A semaphore to be signaled when the image is available.</param>
    /// <returns>True if the swapchain is still valid and the image was acquired, false otherwise.</returns>
    [[nodiscard]] bool advance(const vk::Semaphore &image_available_semaphore);

    /// <summary>
    /// Presents the current image to the screen.
    /// </summary>
    /// <param name="queue">The presentation queue.</param>
    /// <param name="present_info">The present info structure.</param>
    void present(const vk::Queue &queue, vk::PresentInfoKHR &present_info);

private:
    vk::Device mDevice;
    vk::PhysicalDevice mPhysicalDevice;
    vk::SurfaceKHR mSurface;
    glfw::Window mWindow;
    vma::Allocator mAllocator;

    vk::SurfaceFormatKHR mSurfaceFormat = {.format = vk::Format::eUndefined, .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear};
    vk::Format mSurfaceFormatLinear = vk::Format::eUndefined;

    vk::Extent2D mSurfaceExtents = {};
    vk::UniqueSwapchainKHR mSwapchain;
    std::vector<vk::Image> mSwapchainImages;
    std::vector<vk::UniqueImageView> mSwapchainImageViewsSrgb;
    std::vector<vk::UniqueImageView> mSwapchainImageViewsUnorm;

    vma::UniqueImage mDepthImage;
    vma::UniqueAllocation mDepthImageAllocation;
    vk::UniqueImageView mDepthImageView;
    const vk::Format mDepthFormat = vk::Format::eD32Sfloat;

    uint32_t mActiveImageIndex = 0;
    int mImageCount = 0;
    int mMinImageCount = 0;
    int mMaxImageCount = 0;
    vk::PresentModeKHR mPresentMode = vk::PresentModeKHR::eImmediate;
    bool mInvalid = true;
};
