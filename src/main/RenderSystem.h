#pragma once

#include "backend/Descriptors.h"
#include "backend/Framebuffer.h"
#include "backend/ShaderCompiler.h"
#include "backend/VulkanContext.h"
#include "blob/Model.h"
#include "entity/Cubemap.h"
#include "imgui/ImGui.h"
#include "renderer/BlobRenderer.h"
#include "renderer/FinalizeRenderer.h"
#include "renderer/PbrSceneRenderer.h"
#include "renderer/ShadowRenderer.h"
#include "renderer/SkyboxRenderer.h"
#include "util/PerFrame.h"


class ShadowCascade;
struct RenderData {
    const scene::GpuData &gltfScene;
    const Camera &camera;
    const ShadowCascade& sunShadowCasterCascade;
    const DirectionalLight &sunLight;
    const Settings &settings;
    const blob::Model &blobModel;
    const Cubemap &skybox;
};

class RenderSystem {

    struct FramesInFlightSyncObjects {
        vk::UniqueSemaphore availableSemaphore;
        vk::UniqueFence inFlightFence;
    };

    VulkanContext *mContext;

    vk::UniqueCommandPool mCommandPool;


    util::PerFrame<FramesInFlightSyncObjects> mFramesInFlightSyncObjects;
    util::PerFrame<vk::UniqueSemaphore> mRenderFinishedSemaphores;
    util::PerFrame<vk::CommandBuffer> mCommandBuffers;
    util::PerFrame<Framebuffer> mSwapchainFramebuffers;
    util::PerFrame<UniqueDescriptorAllocator> mDescriptorAllocators;
    util::PerFrame<UniqueTransientBufferAllocator> mTransientBufferAllocators;

    // This descriptor allocator is never reset
    UniqueDescriptorAllocator mStaticDescriptorAllocator;
    ShaderLoader mShaderLoader;

    Framebuffer mHdrFramebuffer;
    AttachmentImage mHdrColorAttachment;
    AttachmentImage mHdrDepthAttachment;

    std::unique_ptr<ImGuiBackend> mImguiBackend;

    std::unique_ptr<PbrSceneRenderer> mPbrSceneRenderer;
    std::unique_ptr<ShadowRenderer> mShadowRenderer;
    std::unique_ptr<FinalizeRenderer> mFinalizeRenderer;
    std::unique_ptr<BlobRenderer> mBlobRenderer;
    std::unique_ptr<SkyboxRenderer> mSkyboxRenderer;
    std::unique_ptr<FrustumCuller> mFrustumCuller;


public:
    explicit RenderSystem(VulkanContext *context);

    void recreate();

    void draw(const RenderData &render_data);

    /// <summary>Advance to the next frame</summary>
    void advance();
    /// <summary>Begin recording commands</summary>
    void begin();

    void submit();

    [[nodiscard]] const DescriptorAllocator &staticDescriptorAllocator() const { return mStaticDescriptorAllocator; }
    [[nodiscard]] DescriptorAllocator &staticDescriptorAllocator() { return mStaticDescriptorAllocator; }

    [[nodiscard]] const ShaderLoader &shaderLoader() const { return mShaderLoader; }
    [[nodiscard]] ShaderLoader &shaderLoader() { return mShaderLoader; }

    [[nodiscard]] const ImGuiBackend &imGuiBackend() const { return *mImguiBackend; }
    [[nodiscard]] ImGuiBackend &imGuiBackend() { return *mImguiBackend; }
};
