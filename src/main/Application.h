#pragma once

#include "backend/VulkanContext.h"
#include "backend/Descriptors.h"
#include "backend/Pipeline.h"
#include "imgui/ImGui.h"
#include "util/Logger.h"

class ImGuiBackend;
class ShaderLoader;

struct PerFrameResources {
    uint32_t index;
    vk::CommandBuffer commandBuffer;
    vk::UniqueSemaphore availableSemaphore;
    vk::UniqueSemaphore finishedSemaphore;
    vk::UniqueFence inFlightFence;
    DescriptorSet exampleDescriptorSet;
};

struct alignas(16) ExampleShaderPushConstants {
    float angle;
};

struct alignas(16) ExampleInlineUniformBlock {
    float alpha;
};

struct ExampleDescriptorLayout : DescriptorSetLayout {
    static constexpr InlineUniformBlockBinding InlineUniforms{0, vk::ShaderStageFlagBits::eFragment, sizeof(ExampleInlineUniformBlock)};

    ExampleDescriptorLayout() = default;

    explicit ExampleDescriptorLayout(const vk::Device& device) {
        create(device, {}, InlineUniforms);
    }
};

// Temporary until we make the app more sophisticated
struct AppData {
    std::vector<PerFrameResources> perFrameResources;
    vk::UniqueCommandPool commandPool;
    ConfiguredPipeline pipeline;
    ExampleDescriptorLayout descriptorLayout;
    DescriptorAllocator descriptorAllocator;
    std::unique_ptr<ImGuiBackend> imguiBackend;

    ~AppData() {
        Logger::info("Destroying AppData");
    }
};

class Application {
    // Order is important here
    std::unique_ptr<VulkanContext> mContext;
    AppData mData;

    void createPerFrameResources();
    void createImGuiBackend();

    void createPipeline(const ShaderLoader& loader);
    void recordCommands(const vk::CommandBuffer& cmd_buf , const PerFrameResources& per_frame);

    void drawGui();
    void drawFrame(uint32_t frame_index);

public:
    void init();
    void run();
};
