#include "RenderSystem.h"

#include "backend/Swapchain.h"

RenderSystem::RenderSystem(VulkanContext *context) : mContext(context) {
    mImguiBackend = std::make_unique<ImGuiBackend>(
            context->instance(), context->device(), context->physicalDevice(), context->window(), context->swapchain(),
            context->mainQueue, context->swapchain().depthFormat()
    );
    mCommandPool = context->device().createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = context->mainQueue,
    });
    mDescriptorAllocator = UniqueDescriptorAllocator(context->device());

    mShaderLoader = ShaderLoader();
    mShaderLoader.optimize = true;
#ifndef NDEBUG
    mShaderLoader.debug = true;
#endif
    mPbrSceneRenderer = std::make_unique<PbrSceneRenderer>(context->device(), mDescriptorAllocator);
    mShadowRenderer = std::make_unique<ShadowRenderer>();
    mFinalizeRenderer = std::make_unique<FinalizeRenderer>(context->device(), mDescriptorAllocator);
    mBlobRenderer = std::make_unique<BlobRenderer>();
}

void RenderSystem::recreate() {
    const auto &swapchain = mContext->swapchain();
    const auto &device = mContext->device();
    const auto &cmd_pool = *mCommandPool;

    mHdrColorAttachment = AttachmentImage(
            mContext->allocator(), mContext->device(), vk::Format::eR16G16B16A16Sfloat,
            mContext->swapchain().area().extent, vk::ImageUsageFlagBits::eSampled
    );
    mHdrDepthAttachment = AttachmentImage(
            mContext->allocator(), mContext->device(), vk::Format::eD32Sfloat, mContext->swapchain().area().extent
    );
    mHdrFramebuffer = Framebuffer(mContext->swapchain().area());
    mHdrFramebuffer.depthAttachment = mHdrDepthAttachment;
    mHdrFramebuffer.colorAttachments = {mHdrColorAttachment};

    // I don't really like that recrate has to be called explicitly.
    // I'd prefer an implicit solution, but I couldn't think of a good one right now.
    mPbrSceneRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);
    mShadowRenderer->recreate(device, mShaderLoader);
    mFinalizeRenderer->recreate(device, mShaderLoader);
    mBlobRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);

    // These "need" to match the swapchain image count exactly
    mSyncObjects.create(swapchain.imageCount(), [&] {
        return SyncObjects{
            .availableSemaphore = device.createSemaphoreUnique({}),
            .finishedSemaphore = device.createSemaphoreUnique({}),
            .inFlightFence = device.createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}),
        };
    });
    mCommandBuffers.create(swapchain.imageCount(), [&] {
        return device
                .allocateCommandBuffers(
                        {.commandPool = cmd_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1}
                )
                .at(0);
    });
    mSwapchainFramebuffers.create(swapchain.imageCount(), [&](int i) {
        auto fb = Framebuffer(swapchain.area());
        fb.colorAttachments = {{
            .image = swapchain.colorImage(i),
            .view = swapchain.colorViewLinear(i),
            .format = swapchain.colorFormatLinear(),
            .extents = swapchain.extents(),
            .range = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1},
        }};
        fb.depthAttachment = {
            .image = mContext->swapchain().depthImage(),
            .view = mContext->swapchain().depthView(),
            .format = mContext->swapchain().depthFormat(),
            .extents = swapchain.extents(),
            .range = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1},
        };
        return fb;
    });
}

void RenderSystem::draw(const RenderData &rd) {
    const auto &cmd_buf = mCommandBuffers.get();
    const auto &swapchain = mContext->swapchain();

    // Framebuffer needs to be synced to swapchain, so get it explicitly
    Framebuffer &swapchain_fb = mSwapchainFramebuffers.get(swapchain.activeImageIndex());

    // Shadow pass
    mShadowRenderer->execute(cmd_buf, rd.gltfScene, rd.sunShadowCaster);

    // Main render pass
    mPbrSceneRenderer->execute(
            mContext->device(), cmd_buf, mHdrFramebuffer, rd.camera, rd.gltfScene, rd.sunLight, rd.sunShadowCaster
    );

    // Blob render pass
    mBlobRenderer->execute(cmd_buf, mHdrFramebuffer, rd.camera, rd.blobModel);

    // Post-processing pass
    mFinalizeRenderer->execute(
            mContext->device(), cmd_buf, mHdrFramebuffer.colorAttachments[0], swapchain_fb.colorAttachments[0],
            rd.settings.agx
    );

    // ImGui render pass
    {
        // temporarily change view to linear format to fix an ImGui issue.
        Framebuffer imgui_fb = swapchain_fb;
        imgui_fb.colorAttachments[0].view = swapchain.colorViewLinear();
        cmd_buf.beginRendering(imgui_fb.renderingInfo({}));
        mImguiBackend->render(cmd_buf);
        cmd_buf.endRendering();
    }

    swapchain_fb.colorAttachments[0].barrier(cmd_buf, ImageResourceAccess::PresentSrc);
}

void RenderSystem::begin() {
    auto &swapchain = mContext->swapchain();
    auto &sync_objects = mSyncObjects.next();

    while (mContext->device().waitForFences(*sync_objects.inFlightFence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }

    if (!swapchain.advance(*sync_objects.availableSemaphore)) {
        recreate();
        // Swapchain was re-created, skip this frame
        return;
    }

    vk::CommandBuffer &cmd_buf = mCommandBuffers.next();
    cmd_buf.reset();
    cmd_buf.begin(vk::CommandBufferBeginInfo{});
}

void RenderSystem::submit() {
    auto &cmd_buf = mCommandBuffers.get();
    auto &sync_objects = mSyncObjects.get();

    cmd_buf.end();

    vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info = vk::SubmitInfo()
                                         .setCommandBuffers(cmd_buf)
                                         .setWaitSemaphores(*sync_objects.availableSemaphore)
                                         .setWaitDstStageMask(pipe_stage_flags)
                                         .setSignalSemaphores(*sync_objects.finishedSemaphore);

    mContext->device().resetFences(*sync_objects.inFlightFence);
    mContext->mainQueue->submit({submit_info}, *sync_objects.inFlightFence);

    if (!mContext->swapchain().present(
                mContext->presentQueue, vk::PresentInfoKHR().setWaitSemaphores(*sync_objects.finishedSemaphore)
        )) {
        recreate();
    }
}
