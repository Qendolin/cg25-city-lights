#pragma once

#include <vulkan/vulkan.hpp>

/// <summary>
/// Holds a Vulkan queue and its family index.
/// </summary>
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