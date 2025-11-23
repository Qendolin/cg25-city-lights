#include "Swapchain.h"

#include <GLFW/glfw3.h>
#include <algorithm>

#include "../debug/Annotation.h"
#include "../util/Logger.h"
#include "../util/globals.h"

Swapchain::Swapchain(
    const vk::Device &device,
    const vk::PhysicalDevice &physical_device,
    const vk::SurfaceKHR &surface,
    const glfw::Window &window,
    const vma::Allocator &allocator
)
    : mDevice(device), mPhysicalDevice(physical_device), mSurface(surface), mWindow(window), mAllocator(allocator) { create(); }

void Swapchain::create() {
    auto surface_formats = mPhysicalDevice.getSurfaceFormatsKHR(mSurface);
    auto surface_present_modes = mPhysicalDevice.getSurfacePresentModesKHR(mSurface);

    auto surface_format_iter = std::find_if(surface_formats.begin(), surface_formats.end(), [](auto &&format) {
        return (format.format == vk::Format::eB8G8R8A8Unorm || format.format == vk::Format::eR8G8B8A8Unorm) &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    if (surface_format_iter == surface_formats.end())
        Logger::fatal("No suitable surface format found");
    mSurfaceFormat = surface_format_iter[0];

    auto present_mode_preference = [](const vk::PresentModeKHR mode) {
        if (mode == vk::PresentModeKHR::eMailbox)
            return 3;
        if (mode == vk::PresentModeKHR::eFifoRelaxed)
            return 2;
        if (mode == vk::PresentModeKHR::eFifo)
            return 1;
        if (mode == vk::PresentModeKHR::eImmediate)
            return 0;
        return -1;
    };
    std::ranges::sort(surface_present_modes, [&present_mode_preference](const auto a, const auto b) {
        return present_mode_preference(a) > present_mode_preference(b);
    });
    mPresentMode = surface_present_modes.front();

    if (present_mode_preference(mPresentMode) < 0)
        Logger::fatal("No suitable present mode found");

    // Query surface capabilities when using this specific present mode
    vk::StructureChain surface_capabilities_query = {
        vk::PhysicalDeviceSurfaceInfo2KHR{.surface = mSurface}, vk::SurfacePresentModeEXT{.presentMode = mPresentMode}
    };
    // different present modes can have specific image count requirements
    auto surface_capabilities =
            mPhysicalDevice
            .getSurfaceCapabilities2KHR(surface_capabilities_query.get<vk::PhysicalDeviceSurfaceInfo2KHR>())
            .surfaceCapabilities;

    // +1 avoids stalls when cpu and gpu are fast and waiting on the monitor
    uint32_t swapchain_image_count = util::MaxFramesInFlight + 1;
    if (surface_capabilities.maxImageCount > 0)
        swapchain_image_count = std::min(swapchain_image_count, surface_capabilities.maxImageCount);
    swapchain_image_count = std::max(swapchain_image_count, surface_capabilities.minImageCount);
    mImageCount = static_cast<int>(swapchain_image_count);
    mMinImageCount = static_cast<int>(surface_capabilities.minImageCount);
    mMaxImageCount = static_cast<int>(std::max(surface_capabilities.maxImageCount, swapchain_image_count));

    mSurfaceExtents = mWindow.getFramebufferSize();
    mSurfaceExtents.width = std::clamp(
        mSurfaceExtents.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width
    );
    mSurfaceExtents.height = std::clamp(
        mSurfaceExtents.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height
    );

    // need to be destroyed before swapchain is
    mSwapchainImageViewsUnorm.clear();

    mSwapchain = mDevice.createSwapchainKHRUnique({
        .surface = mSurface,
        .minImageCount = swapchain_image_count,
        .imageFormat = mSurfaceFormat.format,
        .imageColorSpace = mSurfaceFormat.colorSpace,
        .imageExtent = mSurfaceExtents,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = mPresentMode,
        .clipped = true,
        .oldSwapchain = *mSwapchain,
    });

    mSwapchainImages = mDevice.getSwapchainImagesKHR(*mSwapchain);

    for (const auto &swapchain_image: mSwapchainImages) {
        util::setDebugName(mDevice, swapchain_image, "swapchain_image");

        vk::ImageViewCreateInfo image_view_create_info = {
            .image = swapchain_image,
            .viewType = vk::ImageViewType::e2D,
            .format = mSurfaceFormat.format,
            .components = vk::ComponentMapping{},
            .subresourceRange =
            {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        const auto& view = mSwapchainImageViewsUnorm.emplace_back(mDevice.createImageViewUnique(image_view_create_info));
        util::setDebugName(mDevice, *view, "swapchain_image_view");
    }

    std::tie(mDepthImage, mDepthImageAllocation) = mAllocator.createImageUnique(
        vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eD32Sfloat,
            .extent = {.width = mSurfaceExtents.width, .height = mSurfaceExtents.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
        },
        {
            .usage = vma::MemoryUsage::eAutoPreferDevice,
            .requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
        });
    util::setDebugName(mDevice, *mDepthImage, "swapchain_depth_image");

    mDepthImageView = mDevice.createImageViewUnique({
        .image = *mDepthImage,
        .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eD32Sfloat,
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1},
    });
    util::setDebugName(mDevice, *mDepthImageView, "swapchain_depth_image_view");

    mInvalid = false;
}

void Swapchain::recreate() {
    // wait if the window is minimized, it can crash otherwise
    auto extents = mWindow.getFramebufferSize();
    while (extents.width == 0 || extents.height == 0) {
        glfwWaitEvents();
        extents = mWindow.getFramebufferSize();
    }
    mDevice.waitIdle();

    create();
}

bool Swapchain::advance(const vk::Semaphore &image_available_semaphore) {
    auto extents = mWindow.getFramebufferSize();
    if (mSurfaceExtents.width != extents.width || mSurfaceExtents.height != extents.height) {
        Logger::debug("Swapchain needs recreation: framebuffer size changed");
        recreate();
        return false;
    }

    try {
        auto image_acquisition_result =
                mDevice.acquireNextImageKHR(*mSwapchain, UINT64_MAX, image_available_semaphore, nullptr);
        if (image_acquisition_result.result == vk::Result::eSuboptimalKHR) {
            Logger::debug("Swapchain may need recreation: VK_SUBOPTIMAL_KHR");
            invalidate();
        }
        mActiveImageIndex = image_acquisition_result.value;
    } catch (const vk::OutOfDateKHRError &) {
        Logger::debug("Swapchain needs recreation: VK_ERROR_OUT_OF_DATE_KHR");
        invalidate();
    }

    if (mInvalid) {
        recreate();
        return false;
    }
    return true;
}

bool Swapchain::present(const vk::Queue &queue, vk::PresentInfoKHR &present_info) {
    present_info.setSwapchains(*mSwapchain).setImageIndices(mActiveImageIndex);

    try {
        vk::Result result = queue.presentKHR(present_info);
        if (result == vk::Result::eSuboptimalKHR) {
            Logger::debug("Swapchain may need recreation: VK_SUBOPTIMAL_KHR");
            invalidate();
        }
    } catch (const vk::OutOfDateKHRError &) {
        Logger::debug("Swapchain needs recreation: VK_ERROR_OUT_OF_DATE_KHR");
        invalidate();
    }

    if (mInvalid) {
        recreate();
        return false;
    }
    return true;
}
