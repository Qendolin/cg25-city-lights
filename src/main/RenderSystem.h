#pragma once

#include "backend/Descriptors.h"
#include "backend/Framebuffer.h"
#include "backend/ShaderCompiler.h"
#include "backend/VulkanContext.h"
#include "blob/Model.h"
#include "entity/Cubemap.h"
#include "imgui/ImGui.h"
#include "renderer/BlobRenderer.h"
#include "renderer/DepthPrePassRenderer.h"
#include "renderer/FinalizeRenderer.h"
#include "renderer/LightRenderer.h"
#include "renderer/PbrSceneRenderer.h"
#include "renderer/SSAORenderer.h"
#include "renderer/ShadowRenderer.h"
#include "renderer/SkyboxRenderer.h"
#include "util/PerFrame.h"


class ShadowCascade;

struct RenderData {
    const scene::GpuData &gltfScene;
    const Camera &camera;
    const ShadowCascade &sunShadowCasterCascade;
    const DirectionalLight &sunLight;
    const Settings &settings;
    const blob::Model &blobModel;
    const Cubemap &skybox;
    float timestamp;
};

class RenderSystem {

    struct PerFrameObjects {
        vk::CommandBuffer earlyGraphicsCommands;
        vk::CommandBuffer mainGraphicsCommands;
        vk::CommandBuffer independentGraphicsCommands;
        vk::CommandBuffer asyncComputeCommands;
        vk::CommandBuffer nonAsyncComputeCommands;

        vk::UniqueSemaphore earlyGraphicsFinishedSemaphore;
        vk::UniqueSemaphore asyncComputeFinishedSemaphore;
        vk::UniqueSemaphore imageAvailableSemaphore;

        vk::UniqueFence inFlightFence;

        UniqueDescriptorAllocator descriptorAllocator;
        UniqueTransientBufferAllocator transientBufferAllocator;

        void reset(const vk::Device& device);
        void setDebugLabels(const vk::Device& device, int frame);
    };

    struct Timings {
        double total = 0;
        double record = 0;
        double submit = 0;
        double present = 0;
        double fence = 0;
        double advance = 0;
    };

    VulkanContext *mContext;

    vk::UniqueCommandPool mGraphicsCommandPool;
    vk::UniqueCommandPool mComputeCommandPool;

    // Per frame in flight
    util::PerFrame<PerFrameObjects> mPerFrameObjects;
    // Per swapchain image
    util::PerFrame<vk::UniqueSemaphore> mRenderFinishedSemaphore;
    // Per swapchain image
    util::PerFrame<Framebuffer> mSwapchainFramebuffers;

    util::PerFrame<Buffer> mInstanceTransformUpdates;

    // This descriptor allocator is never reset
    UniqueDescriptorAllocator mStaticDescriptorAllocator;
    ShaderLoader mShaderLoader;

    Framebuffer mHdrFramebuffer;
    ImageWithView mHdrColorAttachment;
    ImageWithView mHdrDepthAttachment;
    ImageWithView mStoredHdrColorImage;
    ImageWithView mSsaoIntermediaryImage;
    ImageWithView mSsaoResultImage;
    ImageWithView mHdrColorResolveImage;
    ImageWithView mComputeDepthCopyImage;
    util::PerFrame<Buffer> mTileLightIndicesBuffers;

    std::unique_ptr<ImGuiBackend> mImguiBackend;

    std::unique_ptr<PbrSceneRenderer> mPbrSceneRenderer;
    std::unique_ptr<ShadowRenderer> mShadowRenderer;
    std::unique_ptr<FinalizeRenderer> mFinalizeRenderer;
    std::unique_ptr<BlobRenderer> mBlobRenderer;
    std::unique_ptr<SkyboxRenderer> mSkyboxRenderer;
    std::unique_ptr<FrustumCuller> mFrustumCuller;
    std::unique_ptr<SSAORenderer> mSSAORenderer;
    std::unique_ptr<DepthPrePassRenderer> mDepthPrePassRenderer;
    std::unique_ptr<LightRenderer> mLightRenderer;

    std::chrono::time_point<std::chrono::steady_clock> mBeginTime;
    Timings mTimings;

public:
    explicit RenderSystem(VulkanContext *context);

    void recreate(const Settings &settings);

    void updateInstanceTransforms(const scene::GpuData &gpu_scene_data, std::span<const glm::mat4> updated_transforms);

    void draw(const RenderData &render_data);

    /// <summary>Advance to the next frame</summary>
    void advance(const Settings &settings);
    /// <summary>Begin recording commands</summary>
    void begin();

    void submit(const Settings &settings);

    [[nodiscard]] const DescriptorAllocator &staticDescriptorAllocator() const { return mStaticDescriptorAllocator; }
    [[nodiscard]] DescriptorAllocator &staticDescriptorAllocator() { return mStaticDescriptorAllocator; }

    [[nodiscard]] const ShaderLoader &shaderLoader() const { return mShaderLoader; }
    [[nodiscard]] ShaderLoader &shaderLoader() { return mShaderLoader; }

    [[nodiscard]] const ImGuiBackend &imGuiBackend() const { return *mImguiBackend; }
    [[nodiscard]] ImGuiBackend &imGuiBackend() { return *mImguiBackend; }

    [[nodiscard]] const Timings &timings() const { return mTimings; }

private:

    void resolveHdrColorImage(const vk::CommandBuffer &cmd_buf) const;
    void storeHdrColorImage(const vk::CommandBuffer& cmd_buf) const;
};
