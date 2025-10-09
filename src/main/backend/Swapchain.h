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

class Swapchain {
public:
    Swapchain(const vk::Device& device, const vk::PhysicalDevice& physical_device, const vk::SurfaceKHR& surface, const glfw::Window& window, const vma::Allocator& allocator);

    [[nodiscard]] vk::Format colorFormatSrgb() const { return mSurfaceFormat.format; }

    [[nodiscard]] vk::Format colorFormatLinear() const {
        if (mSurfaceFormatLinear == vk::Format::eUndefined)
            return colorFormatSrgb();
        return mSurfaceFormatLinear;
    }

    [[nodiscard]] vk::Format depthFormat() const { return mDepthFormat; }

    [[nodiscard]] int imageCount() const { return mImageCount; }

    [[nodiscard]] int minImageCount() const { return mMinImageCount; }

    [[nodiscard]] int maxImageCount() const { return mMaxImageCount; }

    [[nodiscard]] vk::PresentModeKHR presentMode() const { return mPresentMode; }

    [[nodiscard]] vk::Extent2D extents() const { return mSurfaceExtents; }

    [[nodiscard]] vk::Rect2D area() const { return {{}, mSurfaceExtents}; }

    [[nodiscard]] float width() const { return static_cast<float>(mSurfaceExtents.width); }

    [[nodiscard]] float height() const { return static_cast<float>(mSurfaceExtents.height); }

    [[nodiscard]] vk::Image colorImage() const { return mSwapchainImages.at(mActiveImageIndex); }

    [[nodiscard]] vk::Image colorImage(int i) const { return mSwapchainImages.at(i); }

    [[nodiscard]] vk::ImageView colorViewSrgb() const { return *mSwapchainImageViewsSrgb.at(mActiveImageIndex); }

    [[nodiscard]] vk::ImageView colorViewSrgb(int i) const { return *mSwapchainImageViewsSrgb.at(i); }

    [[nodiscard]] vk::ImageView colorViewLinear() const {
        if (mSurfaceFormatLinear == vk::Format::eUndefined)
            return colorViewSrgb();
        return *mSwapchainImageViewsUnorm.at(mActiveImageIndex);
    }

    [[nodiscard]] vk::ImageView colorViewLinear(int i) const {
        if (mSurfaceFormatLinear == vk::Format::eUndefined)
            return colorViewSrgb(i);
        return *mSwapchainImageViewsUnorm.at(i);
    }

    [[nodiscard]] vk::Image depthImage() const { return *mDepthImage; }

    [[nodiscard]] vk::ImageView depthView() const { return *mDepthImageView; }

    void create();

    void recreate();

    void invalidate() { mInvalid = true; }

    [[nodiscard]] bool advance(const vk::Semaphore &image_available_semaphore);

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
