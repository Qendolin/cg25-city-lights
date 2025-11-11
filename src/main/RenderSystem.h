#pragma once

#include "backend/Descriptors.h"
#include "backend/Framebuffer.h"
#include "backend/ShaderCompiler.h"
#include "backend/VulkanContext.h"
#include "blob/model/Model.h"
#include "imgui/ImGui.h"
#include "renderer/BlobRenderer.h"
#include "renderer/FinalizeRenderer.h"
#include "renderer/PbrSceneRenderer.h"
#include "renderer/ShadowRenderer.h"
#include "util/PerFrame.h"

struct RenderData {
    const scene::GpuData &gltfScene;
    const Camera &camera;
    const ShadowCaster &sunShadowCaster;
    const DirectionalLight &sunLight;
    const Settings &settings;
    const blob::Model &blobModel;
};

class RenderSystem {

    struct SyncObjects {
        vk::UniqueSemaphore availableSemaphore;
        vk::UniqueSemaphore finishedSemaphore;
        vk::UniqueFence inFlightFence;
    };

    VulkanContext *mContext;

    vk::UniqueCommandPool mCommandPool;

    util::PerFrame<SyncObjects> mSyncObjects;
    util::PerFrame<vk::CommandBuffer> mCommandBuffers;
    util::PerFrame<Framebuffer> mSwapchainFramebuffers;

    UniqueDescriptorAllocator mDescriptorAllocator;

    ShaderLoader mShaderLoader;

    Framebuffer mHdrFramebuffer;
    AttachmentImage mHdrColorAttachment;
    AttachmentImage mHdrDepthAttachment;

    std::unique_ptr<ImGuiBackend> mImguiBackend;

    std::unique_ptr<PbrSceneRenderer> mPbrSceneRenderer;
    std::unique_ptr<ShadowRenderer> mShadowRenderer;
    std::unique_ptr<FinalizeRenderer> mFinalizeRenderer;
    std::unique_ptr<BlobRenderer> mBlobRenderer;

public:
    explicit RenderSystem(VulkanContext *context);

    void recreate();

    void draw(const RenderData &render_data);

    void begin();

    void submit();

    [[nodiscard]] const DescriptorAllocator &descriptorAllocator() const { return mDescriptorAllocator; }
    [[nodiscard]] DescriptorAllocator &descriptorAllocator() { return mDescriptorAllocator; }

    [[nodiscard]] const ShaderLoader &shaderLoader() const { return mShaderLoader; }
    [[nodiscard]] ShaderLoader &shaderLoader() { return mShaderLoader; }

    [[nodiscard]] const ImGuiBackend &imGuiBackend() const { return *mImguiBackend; }
    [[nodiscard]] ImGuiBackend &imGuiBackend() { return *mImguiBackend; }

    // TEMP - NOT GOOD!
    vk::CommandBuffer &getCommandBuffer() { return mCommandBuffers.get(); }
};
