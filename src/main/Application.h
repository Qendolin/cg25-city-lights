#pragma once

#include <vulkan/vulkan.hpp>

#include "util/PerFrame.h"


namespace glfw {
    class Input;
}
class Camera;
class VulkanContext;
class DescriptorAllocator;
class Framebuffer;
class PbrSceneRenderer;
namespace scene {
    class Scene;
}
class ImGuiBackend;
class ShaderLoader;

struct SyncObjects {
    vk::UniqueSemaphore availableSemaphore;
    vk::UniqueSemaphore finishedSemaphore;
    vk::UniqueFence inFlightFence;
};

class Application {
    // Order is important here
    std::unique_ptr<VulkanContext> context;
    vk::UniqueCommandPool commandPool;
    vk::UniqueCommandPool transientTransferCommandPool;
    util::PerFrame<SyncObjects> syncObjects;
    util::PerFrame<vk::CommandBuffer> commandBuffers;
    util::PerFrame<Framebuffer> swapchainFramebuffers;
    std::unique_ptr<DescriptorAllocator> descriptorAllocator;
    std::unique_ptr<ShaderLoader> shaderLoader;

    std::unique_ptr<ImGuiBackend> imguiBackend;
    std::unique_ptr<PbrSceneRenderer> pbrSceneRenderer;

    std::unique_ptr<glfw::Input> input;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<scene::Scene> scene;

    // Called after the swapchain was invalidated, or when shaders are reloaded, and when the application is initialized
    void recreate();

    void recordCommands(const vk::CommandBuffer &cmd_buf, Framebuffer &fb) const;

    void processInput();
    void drawGui();
    void drawFrame();

public:

    Application();
    ~Application();

    void init();
    void run();
};
