#pragma once

#include <glm/mat4x4.hpp>

#include "backend/Descriptors.h"
#include "backend/Pipeline.h"
#include "backend/VulkanContext.h"
#include "imgui/ImGui.h"

namespace scene {
    class Scene;
}
class ImGuiBackend;
class ShaderLoader;

struct PerFrameResources {
    vk::CommandBuffer commandBuffer;
    vk::UniqueSemaphore availableSemaphore;
    vk::UniqueSemaphore finishedSemaphore;
    vk::UniqueFence inFlightFence;
    DescriptorSet descriptorSet;
};

struct alignas(16) SceneInlineUniformBlock {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec4 camera; // Padded to 16 bytes
};

struct PerFrameDescriptorLayout : DescriptorSetLayout {
    static constexpr InlineUniformBlockBinding SceneUniforms{0, vk::ShaderStageFlagBits::eAllGraphics, sizeof(SceneInlineUniformBlock)};

    PerFrameDescriptorLayout() = default;

    explicit PerFrameDescriptorLayout(const vk::Device& device) {
        create(device, {}, SceneUniforms);
    }
};

// Temporary until we make the app more sophisticated
struct AppData {
    std::vector<PerFrameResources> perFrameResources;
    vk::UniqueCommandPool commandPool;
    vk::UniqueCommandPool transientTransferCommandPool;
    ConfiguredPipeline pipeline;
    PerFrameDescriptorLayout perFrameDescriptorLayout;
    DescriptorAllocator descriptorAllocator;
    std::unique_ptr<ImGuiBackend> imguiBackend;
    DescriptorSet sceneDataDescriptorSet;
    std::unique_ptr<scene::Scene> scene;

    AppData() noexcept;

    ~AppData();
};

class Application {
    // Order is important here
    std::unique_ptr<VulkanContext> mContext;
    AppData mData;

    void createPerFrameResources();
    void createImGuiBackend();

    void createPipeline(const ShaderLoader &loader);
    void recordCommands(const vk::CommandBuffer& cmd_buf , const PerFrameResources& per_frame);

    void drawGui();
    void drawFrame(uint32_t frame_index);

public:
    void init();
    void run();
};
