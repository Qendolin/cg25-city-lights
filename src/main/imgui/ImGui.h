#pragma once

// ReSharper disable once CppUnusedIncludeDirective
#include <imgui.h>
#include <utility>

struct DeviceQueue;
class Swapchain;

namespace vk {
    enum class Format;
    class Instance;
    class Device;
    class PhysicalDevice;
}

namespace vk {
    class CommandBuffer;
}

namespace glfw {
    class Window;
}

class ImGuiBackend {

public:
    ImGuiBackend(
        const vk::Instance& instance,
        const vk::Device& device,
        const vk::PhysicalDevice& physical_device,
        const glfw::Window &window,
        const Swapchain &swapchain,
        const DeviceQueue& queue,
        const vk::Format &depth_format
    );

    ImGuiBackend();
    ~ImGuiBackend();

    ImGuiBackend(const ImGuiBackend &other) = delete;

    ImGuiBackend(ImGuiBackend &&other) noexcept {
        this->mContext = std::exchange(other.mContext, nullptr);
    }

    ImGuiBackend & operator=(const ImGuiBackend &other) = delete;

    ImGuiBackend & operator=(ImGuiBackend &&other) noexcept {
        if (this == &other)
            return *this;
        this->mContext = std::exchange(other.mContext, nullptr);
        return *this;
    }

    void begin() const;

    void render(const vk::CommandBuffer &cmd_buf) const;

private:
    ImGuiContext* mContext = nullptr;
};
