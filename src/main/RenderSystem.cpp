#include "RenderSystem.h"

#include "backend/Swapchain.h"
#include "debug/Annotation.h"
#include "entity/ShadowCaster.h"
#include "util/globals.h"

RenderSystem::RenderSystem(VulkanContext *context) : mContext(context) {
    mImguiBackend = std::make_unique<ImGuiBackend>(
            context->instance(), context->device(), context->physicalDevice(), context->window(), context->swapchain(),
            context->mainQueue, context->swapchain().depthFormat()
    );
    mCommandPool = context->device().createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = context->mainQueue,
    });
    mStaticDescriptorAllocator = UniqueDescriptorAllocator(context->device());

    mShaderLoader = ShaderLoader();
    mShaderLoader.optimize = true;
#ifndef NDEBUG
    mShaderLoader.debug = true;
#endif
    mPbrSceneRenderer = std::make_unique<PbrSceneRenderer>(context->device());
    mShadowRenderer = std::make_unique<ShadowRenderer>();
    mFinalizeRenderer = std::make_unique<FinalizeRenderer>(context->device());
    mBlobRenderer = std::make_unique<BlobRenderer>(context->device());
    mSkyboxRenderer = std::make_unique<SkyboxRenderer>(context->device());
    mFrustumCuller = std::make_unique<FrustumCuller>(context->device());
    mSSAORenderer = std::make_unique<SSAORenderer>(context->device(), context->allocator(), context->mainQueue);
    mDepthPrePassRenderer = std::make_unique<DepthPrePassRenderer>();
}

void RenderSystem::recreate() {
    const auto &swapchain = mContext->swapchain();
    const auto &device = mContext->device();
    const auto &cmd_pool = *mCommandPool;

    vk::Extent2D screen_extent = mContext->swapchain().area().extent;
    vk::Extent2D screen_half_extent = {screen_extent.width / 2, screen_extent.height / 2};

    mHdrColorAttachment = AttachmentImage(
            mContext->allocator(), device, vk::Format::eR16G16B16A16Sfloat, screen_extent, vk::ImageUsageFlagBits::eSampled
    );
    util::setDebugName(device, mHdrColorAttachment.image(), "hdr_color_attachment_image");
    util::setDebugName(device, mHdrColorAttachment.view(), "hdr_color_attachment_image_view");
    mHdrDepthAttachment = AttachmentImage(
            mContext->allocator(), device, vk::Format::eD32Sfloat, screen_extent, vk::ImageUsageFlagBits::eSampled
    );
    util::setDebugName(device, mHdrDepthAttachment.image(), "hdr_depth_attachment_image");
    util::setDebugName(device, mHdrDepthAttachment.view(), "hdr_depth_attachment_image_view");
    mHdrFramebuffer = Framebuffer(mContext->swapchain().area());
    mHdrFramebuffer.depthAttachment = mHdrDepthAttachment;
    mHdrFramebuffer.colorAttachments = {mHdrColorAttachment};

    // TODO: dont use attachment type because image is never used as one
    mSsaoRawAttachment = AttachmentImage(
            mContext->allocator(), device, vk::Format::eR8Unorm, screen_half_extent,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
    );
    util::setDebugName(device, mSsaoRawAttachment.image(), "ao_raw_attachment_image");
    util::setDebugName(device, mSsaoRawAttachment.view(), "ao_raw_attachment_image_view");

    mSsaoFilteredAttachment = AttachmentImage(
            mContext->allocator(), device, vk::Format::eR8Unorm, screen_half_extent,
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage
    );
    util::setDebugName(device, mSsaoFilteredAttachment.image(), "ao_filtered_attachment_image");
    util::setDebugName(device, mSsaoFilteredAttachment.view(), "ao_filtered_attachment_image_view");

    // I don't really like that recrate has to be called explicitly.
    // I'd prefer an implicit solution, but I couldn't think of a good one right now.
    mPbrSceneRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);
    mShadowRenderer->recreate(device, mShaderLoader);
    mFinalizeRenderer->recreate(device, mShaderLoader);
    mBlobRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);
    mSkyboxRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);
    mFrustumCuller->recreate(device, mShaderLoader);
    mSSAORenderer->recreate(device, mShaderLoader);
    mDepthPrePassRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);

    // These have to match the max frames in flight count but an be more
    mFramesInFlightSyncObjects.create(util::MaxFramesInFlight, [&] {
        return FramesInFlightSyncObjects{
            .availableSemaphore = device.createSemaphoreUnique({}),
            .inFlightFence = device.createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}),
        };
    });

    // These have to match the swapchain image count exactly
    mRenderFinishedSemaphores.create(swapchain.imageCount(), [&] { return device.createSemaphoreUnique({}); });
    mCommandBuffers.create(swapchain.imageCount(), [&](int i) {
        auto buf = device.allocateCommandBuffers({.commandPool = cmd_pool,
                                                  .level = vk::CommandBufferLevel::ePrimary,
                                                  .commandBufferCount = 1})
                           .at(0);
        util::setDebugName(device, buf, std::format("per_frame_command_buffer_{}", i));
        return buf;
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
        // Not a pretty solution :(
        fb.colorAttachments[0].setBarrierState(
                {.stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput, .access = vk::AccessFlagBits2::eMemoryRead}
        );
        fb.depthAttachment = {
            .image = mContext->swapchain().depthImage(),
            .view = mContext->swapchain().depthView(),
            .format = mContext->swapchain().depthFormat(),
            .extents = swapchain.extents(),
            .range = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1},
        };
        fb.depthAttachment.setBarrierState(
                {.stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput, .access = vk::AccessFlagBits2::eMemoryRead}
        );
        return fb;
    });
    mDescriptorAllocators.create(swapchain.imageCount(), [&] { return UniqueDescriptorAllocator(device); });
    mTransientBufferAllocators.create(swapchain.imageCount(), [&] {
        return UniqueTransientBufferAllocator(mContext->device(), mContext->allocator());
    });
}

void RenderSystem::draw(const RenderData &rd) {
    const auto &cmd_buf = mCommandBuffers.get();
    const auto &desc_alloc = mDescriptorAllocators.get();
    const auto &buf_alloc = mTransientBufferAllocators.get();
    const auto &swapchain = mContext->swapchain();

    // Framebuffer needs to be synced to swapchain, so get it explicitly
    Framebuffer &swapchain_fb = mSwapchainFramebuffers.get(swapchain.activeImageIndex());

    util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Depth PrePass");
    // Depth pre-pass
    mDepthPrePassRenderer->enableCulling = rd.settings.rendering.enableFrustumCulling;
    mDepthPrePassRenderer->pauseCulling = rd.settings.rendering.pauseFrustumCulling;
    mDepthPrePassRenderer->execute(
            mContext->device(), desc_alloc, buf_alloc, cmd_buf, mHdrFramebuffer, rd.camera, rd.gltfScene, *mFrustumCuller
    );

    dbg_cmd_label_region.swap("SSAO Pass");
    mSSAORenderer->radius = rd.settings.ssao.radius;
    mSSAORenderer->exponent = rd.settings.ssao.exponent;
    mSSAORenderer->bias = rd.settings.ssao.bias;
    mSSAORenderer->filterSharpness = rd.settings.ssao.filterSharpness;
    mSSAORenderer->execute(
            mContext->device(), desc_alloc, cmd_buf, rd.camera.projectionMatrix(), rd.camera.nearPlane(),
            mHdrFramebuffer.depthAttachment, mSsaoRawAttachment, mSsaoFilteredAttachment
    );

    // Shadow pass
    dbg_cmd_label_region.swap("Shadow Pass");
    for (auto &caster: rd.sunShadowCasterCascade.cascades()) {
        mShadowRenderer->execute(mContext->device(), desc_alloc, buf_alloc, cmd_buf, rd.gltfScene, *mFrustumCuller, caster);
    }

    // Main render pass
    dbg_cmd_label_region.swap("Main Pass");
    mPbrSceneRenderer->enableCulling = rd.settings.rendering.enableFrustumCulling;
    mPbrSceneRenderer->pauseCulling = rd.settings.rendering.pauseFrustumCulling;
    mPbrSceneRenderer->execute(
            mContext->device(), desc_alloc, buf_alloc, cmd_buf, mHdrFramebuffer, rd.camera, rd.gltfScene,
            *mFrustumCuller, rd.sunLight, rd.sunShadowCasterCascade.cascades(), mSsaoFilteredAttachment, rd.settings
    );

    // Blob render pass
    dbg_cmd_label_region.swap("Blob Pass");
    mBlobRenderer->execute(mContext->device(), desc_alloc, cmd_buf, mHdrFramebuffer, rd.camera, rd.blobModel);

    // Skybox render pass (render late to reduce overdraw)
    dbg_cmd_label_region.swap("Skybox Pass");
    mSkyboxRenderer->execute(
            mContext->device(), desc_alloc, cmd_buf, mHdrFramebuffer, rd.camera, rd.skybox, rd.settings.sky.exposure,
            rd.settings.sky.tint
    );

    // Post-processing pass
    dbg_cmd_label_region.swap("Post-Process Pass");
    mFinalizeRenderer->execute(
            mContext->device(), desc_alloc, cmd_buf, mHdrFramebuffer.colorAttachments[0],
            swapchain_fb.colorAttachments[0], rd.settings.agx
    );

    // ImGui render pass
    dbg_cmd_label_region.swap("ImGUI Pass");
    {
        swapchain_fb.colorAttachments[0].barrier(
                cmd_buf, ImageResourceAccess::ColorAttachmentLoad, ImageResourceAccess::ColorAttachmentWrite
        );
        // temporarily change view to linear format to fix an ImGui issue.
        Framebuffer imgui_fb = swapchain_fb;
        imgui_fb.colorAttachments[0].view = swapchain.colorViewLinear();
        cmd_buf.beginRendering(imgui_fb.renderingInfo({}));
        mImguiBackend->render(cmd_buf);
        cmd_buf.endRendering();
    }

    dbg_cmd_label_region.end();

    swapchain_fb.colorAttachments[0].barrier(cmd_buf, ImageResourceAccess::PresentSrc);
}

void RenderSystem::advance() {
    auto &swapchain = mContext->swapchain();
    const auto &sync_objects = mFramesInFlightSyncObjects.next();

    while (mContext->device().waitForFences(*sync_objects.inFlightFence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }
    mContext->device().resetFences(*sync_objects.inFlightFence);

    if (!swapchain.advance(*sync_objects.availableSemaphore)) {
        recreate();
    }

    mCommandBuffers.next();
    mDescriptorAllocators.next().reset();
    mTransientBufferAllocators.next().reset();
}

void RenderSystem::begin() {
    vk::CommandBuffer &cmd_buf = mCommandBuffers.get();
    cmd_buf.reset();
    cmd_buf.begin(vk::CommandBufferBeginInfo{});
}

void RenderSystem::submit() {
    const auto &cmd_buf = mCommandBuffers.get();
    const auto &sync_objects = mFramesInFlightSyncObjects.get();
    // These must correspond to the active swapchain image index
    const auto &render_finished_semaphore = mRenderFinishedSemaphores.get(mContext->swapchain().activeImageIndex());

    cmd_buf.end();

    vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info = vk::SubmitInfo()
                                         .setCommandBuffers(cmd_buf)
                                         .setWaitSemaphores(*sync_objects.availableSemaphore)
                                         .setWaitDstStageMask(pipe_stage_flags)
                                         .setSignalSemaphores(*render_finished_semaphore);

    mContext->mainQueue->submit({submit_info}, *sync_objects.inFlightFence);

    if (!mContext->swapchain().present(
                mContext->presentQueue, vk::PresentInfoKHR().setWaitSemaphores(*render_finished_semaphore)
        )) {
        recreate();
    }
}
