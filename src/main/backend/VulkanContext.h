#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "../glfw/Window.h"
#include "DeviceQueue.h"

namespace glfw {
    class Window;
}

class Swapchain;

/// <summary>
/// Manages the core Vulkan objects for the application, including the instance, device, allocator, and window surface.
/// </summary>
class VulkanContext {
public:
    /// <summary>
    /// The main queue for graphics and compute operations.
    /// </summary>
    DeviceQueue mainQueue = {};
    /// <summary>
    /// The queue for async compute operations.
    /// </summary>
    DeviceQueue computeQueue = {};
    /// <summary>
    /// The queue for presenting to the screen.
    /// </summary>
    DeviceQueue presentQueue = {};
    /// <summary>
    /// The queue for transfer operations.
    /// </summary>
    DeviceQueue transferQueue = {};

    /// <summary>
    /// Gets the Vulkan instance.
    /// </summary>
    const vk::Instance &instance() const {
        return *mInstance;
    }

    /// <summary>
    /// Gets the physical device.
    /// </summary>
    const vk::PhysicalDevice &physicalDevice() const {
        return mPhysicalDevice;
    }

    /// <summary>
    /// Gets the logical device.
    /// </summary>
    const vk::Device &device() const {
        return *mDevice;
    }

    /// <summary>
    /// Gets the Vulkan memory allocator.
    /// </summary>
    const vma::Allocator &allocator() const {
        return *mAllocator;
    }

    /// <summary>
    /// Gets the window surface.
    /// </summary>
    const vk::SurfaceKHR &surface() const {
        return *mSurface;
    }

    /// <summary>
    /// Gets the swapchain.
    /// </summary>
    Swapchain &swapchain() {
        return *mSwapchain;
    };

    /// <summary>
    /// Gets the swapchain.
    /// </summary>
    const Swapchain &swapchain() const {
        return *mSwapchain;
    };

    /// <summary>
    /// Gets the application window.
    /// </summary>
    const glfw::Window &window() const {
        return *mWindow;
    }

    VulkanContext() = default;

    ~VulkanContext();

    VulkanContext(const VulkanContext &other) = delete;

    VulkanContext(VulkanContext &&other) noexcept = default;

    VulkanContext &operator=(const VulkanContext &other) = delete;

    VulkanContext &operator=(VulkanContext &&other) noexcept = default;

    /// <summary>
    /// Creates a new VulkanContext.
    /// </summary>
    /// <param name="window_create_info">The window creation info.</param>
    /// <returns>A new VulkanContext.</returns>
    static VulkanContext create(const glfw::WindowCreateInfo &window_create_info);

private:
    // order is important here
    std::unique_ptr<glfw::Window> mWindow;
    vk::UniqueInstance mInstance = {};
    vk::UniqueDebugUtilsMessengerEXT mDebugMessenger = {};
    vk::UniqueSurfaceKHR mSurface = {};
    vk::PhysicalDevice mPhysicalDevice = {};
    vk::UniqueDevice mDevice = {};
    vma::UniqueAllocator mAllocator = {};
    std::unique_ptr<Swapchain> mSwapchain;

    VulkanContext(
        vk::UniqueInstance &&instance,
        vk::UniqueDebugUtilsMessengerEXT &&debug_messenger,
        const vk::PhysicalDevice &physical_device,
        vk::UniqueDevice &&device,
        vma::UniqueAllocator &&allocator,
        std::unique_ptr<glfw::Window> &&window,
        vk::UniqueSurfaceKHR &&surface,
        std::unique_ptr<Swapchain> &&swapchain,
        const DeviceQueue &main_queue,
        const DeviceQueue &compute_queue,
        const DeviceQueue &present_queue,
        const DeviceQueue &transfer_queue);
};
