#pragma once

#include "backend/Descriptors.h"
#include "backend/Pipeline.h"
#include "backend/VulkanContext.h"
#include "imgui/ImGui.h"
#include "scene/GltfLoader.h"
#include "util/Logger.h"

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

struct alignas(16) InstanceBlock {
    glm::mat4 transform;
    glm::uint material;
};

struct alignas(16) MaterialBlock {
    glm::vec4 albedoFactors;
    glm::vec4 mrnFactors; // Padded to 16 bytes
};

struct PerFrameDescriptorLayout : DescriptorSetLayout {
    static constexpr InlineUniformBlockBinding SceneUniforms{0, vk::ShaderStageFlagBits::eAllGraphics, sizeof(SceneInlineUniformBlock)};

    PerFrameDescriptorLayout() = default;

    explicit PerFrameDescriptorLayout(const vk::Device& device) {
        create(device, {}, SceneUniforms);
    }
};

struct SceneDataDescriptorLayout : DescriptorSetLayout {
    static constexpr StorageBufferBinding InstanceBuffer{0, vk::ShaderStageFlagBits::eAllGraphics};
    static constexpr StorageBufferBinding MaterialBuffer{1, vk::ShaderStageFlagBits::eAllGraphics};

    SceneDataDescriptorLayout() = default;

    explicit SceneDataDescriptorLayout(const vk::Device& device) {
        create(device, {}, InstanceBuffer, MaterialBuffer);
    }
};

struct SceneRenderData {
    vma::UniqueBuffer positions;
    vma::UniqueAllocation positionsAlloc;
    vma::UniqueBuffer normals;
    vma::UniqueAllocation normalsAlloc;
    vma::UniqueBuffer tangents;
    vma::UniqueAllocation tangentsAlloc;
    vma::UniqueBuffer texcoords;
    vma::UniqueAllocation texcoordsAlloc;
    vma::UniqueBuffer indices;
    vma::UniqueAllocation indicesAlloc;

    vma::UniqueBuffer instances;
    vma::UniqueAllocation instancesAlloc;

    vma::UniqueBuffer materials;
    vma::UniqueAllocation materialsAlloc;

    vma::UniqueBuffer drawCommands;
    vma::UniqueAllocation drawCommandsAlloc;
};

// Temporary until we make the app more sophisticated
struct AppData {
    std::vector<PerFrameResources> perFrameResources;
    vk::UniqueCommandPool commandPool;
    vk::UniqueCommandPool transientTransferCommandPool;
    ConfiguredPipeline pipeline;
    PerFrameDescriptorLayout perFrameDescriptorLayout;
    SceneDataDescriptorLayout sceneDataDescriptorLayout;
    DescriptorAllocator descriptorAllocator;
    std::unique_ptr<ImGuiBackend> imguiBackend;
    SceneRenderData sceneRenderData;
    DescriptorSet sceneDataDescriptorSet;
};

class Application {
    // Order is important here
    std::unique_ptr<VulkanContext> mContext;
    AppData mData;

    void createPerFrameResources();
    void createImGuiBackend();

    void createPipeline(const ShaderLoader &loader);
    SceneRenderData uploadSceneData(const SceneData &scene_data);
    void recordCommands(const vk::CommandBuffer& cmd_buf , const PerFrameResources& per_frame);

    void drawGui();
    void drawFrame(uint32_t frame_index);

public:
    void init();
    void run();
};
