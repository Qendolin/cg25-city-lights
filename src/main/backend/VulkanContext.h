#pragma once

#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "../glfw/Window.h"

namespace glfw {
    class Window;
}

class Swapchain;

struct DeviceQueue {
    vk::Queue queue = {};
    uint32_t family = {};

    operator vk::Queue() const { return queue; } // NOLINT(*-explicit-constructor)
    operator VkQueue() const { return queue; } // NOLINT(*-explicit-constructor)
    operator uint32_t() const { return family; } // NOLINT(*-explicit-constructor)

    const vk::Queue *operator->() const noexcept {
        return &queue;
    }

    vk::Queue *operator->() noexcept {
        return &queue;
    }
};

class VulkanContext {
public:
    DeviceQueue mainQueue = {};
    DeviceQueue presentQueue = {};
    DeviceQueue transferQueue = {};

    const vk::Instance &instance() const {
        return *mInstance;
    }

    const vk::PhysicalDevice &physicalDevice() const {
        return mPhysicalDevice;
    }

    const vk::Device &device() const {
        return *mDevice;
    }

    const vma::Allocator &allocator() const {
        return *mAllocator;
    }

    const vk::SurfaceKHR &surface() const {
        return *mSurface;
    }

    Swapchain &swapchain() {
        return *mSwapchain;
    };

    const Swapchain &swapchain() const {
        return *mSwapchain;
    };

    const glfw::Window &window() const {
        return *mWindow;
    }

    VulkanContext() = default;

    ~VulkanContext();

    VulkanContext(const VulkanContext &other) = delete;

    VulkanContext(VulkanContext &&other) noexcept;

    VulkanContext &operator=(const VulkanContext &other) = delete;

    VulkanContext &operator=(VulkanContext &&other) noexcept;

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
        const DeviceQueue &present_queue,
        const DeviceQueue &transfer_queue);
};
