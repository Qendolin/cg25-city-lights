#include "RenderSystem.h"

#include "backend/Swapchain.h"
#include "blob/System.h"
#include "debug/Annotation.h"
#include "entity/ShadowCaster.h"
#include "scene/Scene.h"
#include "util/globals.h"
#include "util/math.h"

void RenderSystem::PerFrameObjects::reset(const vk::Device &device) {
    device.resetFences(*inFlightFence);

    earlyGraphicsCommands.reset();
    mainGraphicsCommands.reset();
    independentGraphicsCommands.reset();
    asyncComputeCommands.reset();
    nonAsyncComputeCommands.reset();

    descriptorAllocator.reset();
    transientBufferAllocator.reset();
}

void RenderSystem::PerFrameObjects::setDebugLabels(const vk::Device &device, int frame) {
    util::setDebugName(device, earlyGraphicsCommands, std::format("early_graphics_{}", frame));
    util::setDebugName(device, mainGraphicsCommands, std::format("main_graphics_{}", frame));
    util::setDebugName(device, independentGraphicsCommands, std::format("independent_graphics_{}", frame));
    util::setDebugName(device, asyncComputeCommands, std::format("async_compute_{}", frame));
    util::setDebugName(device, nonAsyncComputeCommands, std::format("non_async_compute_{}", frame));
    util::setDebugName(device, *inFlightFence, std::format("in_flight_{}", frame));
    util::setDebugName(device, *imageAvailableSemaphore, std::format("image_available_{}", frame));
    util::setDebugName(device, *asyncComputeFinishedSemaphore, std::format("async_compute_finished_{}", frame));
    util::setDebugName(device, *earlyGraphicsFinishedSemaphore, std::format("early_graphics_finished{}", frame));
}

RenderSystem::RenderSystem(VulkanContext *context) : mContext(context) {
    mImguiBackend = std::make_unique<ImGuiBackend>(
            context->instance(), context->device(), context->physicalDevice(), context->window(), context->swapchain(),
            context->mainQueue, context->swapchain().depthFormat()
    );
    mGraphicsCommandPool = context->device().createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = context->mainQueue,
    });
    mComputeCommandPool = context->device().createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = context->computeQueue,
    });
    mStaticDescriptorAllocator = UniqueDescriptorAllocator(context->device());

    mShaderLoader = ShaderLoader();
    mShaderLoader.optimize = true;
    if (globals::Debug) {
        mShaderLoader.debug = true;
    }
    mPbrSceneRenderer = std::make_unique<PbrSceneRenderer>(context->device());
    mShadowRenderer = std::make_unique<ShadowRenderer>();
    mFinalizeRenderer = std::make_unique<FinalizeRenderer>(context->device());
    mBlobRenderer = std::make_unique<BlobRenderer>(context->device());
    mSkyboxRenderer = std::make_unique<SkyboxRenderer>(context->device());
    mFrustumCuller = std::make_unique<FrustumCuller>(context->device());
    mSSAORenderer = std::make_unique<SSAORenderer>(context->device(), context->allocator(), context->mainQueue);
    mDepthPrePassRenderer = std::make_unique<DepthPrePassRenderer>();
    mLightRenderer = std::make_unique<LightRenderer>(context->device());
    mFogRenderer = std::make_unique<FogRenderer>(context->device());
    mBloomRenderer = std::make_unique<BloomRenderer>(context->device());
}

void RenderSystem::recreate(const Settings &settings) {
    const auto &swapchain = mContext->swapchain();
    const auto &device = mContext->device();

    vk::Extent2D screen_extent = mContext->swapchain().area().extent;
    vk::Extent2D screen_half_extent = {screen_extent.width / 2, screen_extent.height / 2};

    vk::SampleCountFlagBits msaa_samples = vk::SampleCountFlagBits::e1;
    if (settings.rendering.msaa == 2)
        msaa_samples = vk::SampleCountFlagBits::e2;
    else if (settings.rendering.msaa == 4)
        msaa_samples = vk::SampleCountFlagBits::e4;
    else if (settings.rendering.msaa == 8)
        msaa_samples = vk::SampleCountFlagBits::e8;

    mHdrColorAttachment = ImageWithView::create(
            device, mContext->allocator(),
            {
                .format = vk::Format::eR16G16B16A16Sfloat,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .samples = msaa_samples,
                .width = screen_extent.width,
                .height = screen_extent.height,
                .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                         vk::ImageUsageFlagBits::eTransferSrc,
                .device = vma::MemoryUsage::eGpuOnly,
            }
    );
    util::setDebugName(device, *mHdrColorAttachment.image, "hdr_color_attachment_image");
    util::setDebugName(device, *mHdrColorAttachment.view, "hdr_color_attachment_image_view");
    if (msaa_samples != vk::SampleCountFlagBits::e1) {
        mHdrColorResolveImage = ImageWithView::create(
                device, mContext->allocator(),
                {
                    .format = vk::Format::eR16G16B16A16Sfloat,
                    .aspects = vk::ImageAspectFlagBits::eColor,
                    .width = screen_extent.width,
                    .height = screen_extent.height,
                    .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled |
                             vk::ImageUsageFlagBits::eStorage,
                    .device = vma::MemoryUsage::eGpuOnly,
                }
        );
        util::setDebugName(device, *mHdrColorResolveImage.image, "hdr_color_resolve_image");
        util::setDebugName(device, *mHdrColorResolveImage.view, "hdr_color_resolve_image_view");
    } else {
        mHdrColorResolveImage = {};
    }
    mHdrDepthAttachment = ImageWithView::create(
            device, mContext->allocator(),
            {
                .format = vk::Format::eD32Sfloat,
                .aspects = vk::ImageAspectFlagBits::eDepth,
                .samples = msaa_samples,
                .width = screen_extent.width,
                .height = screen_extent.height,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled |
                         vk::ImageUsageFlagBits::eTransferSrc,
                .device = vma::MemoryUsage::eGpuOnly,
            }
    );
    util::setDebugName(device, *mHdrDepthAttachment.image, "hdr_depth_attachment_image");
    util::setDebugName(device, *mHdrDepthAttachment.view, "hdr_depth_attachment_image_view");
    mHdrFramebuffer = Framebuffer(mContext->swapchain().area());
    mHdrFramebuffer.depthAttachment = ImageViewPair(mHdrDepthAttachment);
    mHdrFramebuffer.colorAttachments = {ImageViewPair(mHdrColorAttachment)};

    mStoredHdrColorImage = ImageWithView::create(
            device, mContext->allocator(),
            {
                .format = vk::Format::eB10G11R11UfloatPack32,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .width = util::nextLowestPowerOfTwo(screen_extent.width),
                .height = util::nextLowestPowerOfTwo(screen_extent.height),
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
                .device = vma::MemoryUsage::eGpuOnly,
            }
    );
    util::setDebugName(device, *mStoredHdrColorImage.image, "stored_hdr_color_image");
    util::setDebugName(device, *mStoredHdrColorImage.view, "stored_hdr_color_image_view");

    auto ao_size = settings.ssao.halfResolution ? screen_half_extent : screen_extent;
    mSsaoIntermediaryImage = ImageWithView::create(
            device, mContext->allocator(),
            {
                .format = settings.ssao.bentNormals ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8Unorm,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .width = ao_size.width,
                .height = ao_size.height,
                .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                .device = vma::MemoryUsage::eGpuOnly,
            }
    );
    util::setDebugName(device, *mSsaoIntermediaryImage.image, "ao_intermediary_image");
    util::setDebugName(device, *mSsaoIntermediaryImage.view, "ao_intermediary_image_view");

    mSsaoResultImage = ImageWithView::create(
            device, mContext->allocator(),
            {
                .format = settings.ssao.bentNormals ? vk::Format::eR8G8B8A8Unorm : vk::Format::eR8Unorm,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .width = ao_size.width,
                .height = ao_size.height,
                .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                .device = vma::MemoryUsage::eGpuOnly,
                .sharedQueues = {mContext->mainQueue, mContext->computeQueue},
            }
    );
    util::setDebugName(device, *mSsaoResultImage.image, "ao_result_image");
    util::setDebugName(device, *mSsaoResultImage.view, "ao_result_image_view");

    mComputeDepthCopyImage = ImageWithView::create(
            device, mContext->allocator(),
            {
                .format = mHdrFramebuffer.depthFormat(),
                .aspects = vk::ImageAspectFlagBits::eDepth,
                .width = screen_extent.width,
                .height = screen_extent.height,
                .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                         vk::ImageUsageFlagBits::eDepthStencilAttachment,
                .device = vma::MemoryUsage::eGpuOnly,
                .sharedQueues = {mContext->mainQueue, mContext->computeQueue},
            }
    );
    util::setDebugName(device, *mComputeDepthCopyImage.image, "compute_depth_copy_image");
    util::setDebugName(device, *mComputeDepthCopyImage.view, "compute_depth_copy_image_view");

    // I don't really like that recrate has to be called explicitly.
    // I'd prefer an implicit solution, but I couldn't think of a good one right now.
    mPbrSceneRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);
    mShadowRenderer->recreate(device, mShaderLoader);
    mFinalizeRenderer->recreate(device, mShaderLoader);
    mBlobRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);
    mSkyboxRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);
    mFrustumCuller->recreate(device, mShaderLoader);
    mSSAORenderer->recreate(device, mShaderLoader, settings.ssao.slices, settings.ssao.samples, settings.ssao.bentNormals);
    mDepthPrePassRenderer->recreate(device, mShaderLoader, mHdrFramebuffer);
    mLightRenderer->recreate(device, mShaderLoader);
    mFogRenderer->recreate(device, mShaderLoader);
    mBloomRenderer->recreate(device, mContext->allocator(), mShaderLoader, screen_extent);

    // These have to match the max frames in flight count
    if (!mPerFrameObjects.initialized()) {
        mPerFrameObjects.create(globals::MaxFramesInFlight, [&](int i) {
            auto graphics_cmd_bufs = device.allocateCommandBuffers(
                    {.commandPool = *mGraphicsCommandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 4}
            );
            auto compute_cmd_bufs = device.allocateCommandBuffers(
                    {.commandPool = *mComputeCommandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1}
            );

            PerFrameObjects result = {
                .earlyGraphicsCommands = graphics_cmd_bufs[0],
                .mainGraphicsCommands = graphics_cmd_bufs[1],
                .independentGraphicsCommands = graphics_cmd_bufs[2],
                .asyncComputeCommands = compute_cmd_bufs[0],
                .nonAsyncComputeCommands = graphics_cmd_bufs[3],
                .earlyGraphicsFinishedSemaphore = device.createSemaphoreUnique({}),
                .asyncComputeFinishedSemaphore = device.createSemaphoreUnique({}),
                .imageAvailableSemaphore = device.createSemaphoreUnique({}),
                .inFlightFence = device.createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}),
                .descriptorAllocator = UniqueDescriptorAllocator(device),
                .transientBufferAllocator = UniqueTransientBufferAllocator(mContext->device(), mContext->allocator()),
            };
            result.setDebugLabels(device, i);
            return result;
        });
    }

    // These have to match the swapchain image count exactly
    if (!mRenderFinishedSemaphore.initialized()) {
        mRenderFinishedSemaphore.create(swapchain.imageCount(), [&](int i) {
            auto result = device.createSemaphoreUnique({});
            util::setDebugName(device, *result, std::format("render_finished_semaphore_{}", i));
            return result;
        });
    }

    mSwapchainFramebuffers.create(swapchain.imageCount(), [&](int i) {
        auto fb = Framebuffer(swapchain.area());
        fb.colorAttachments = {ImageViewPair(swapchain.colorImage(i), swapchain.colorViewLinear(i))};
        // Not the best solution :(
        swapchain.colorImage(i).setBarrierState(
                {.stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput, .access = vk::AccessFlagBits2::eMemoryRead}
        );
        fb.depthAttachment = ImageViewPair(mContext->swapchain().depthImage(), mContext->swapchain().depthView());
        return fb;
    });

    size_t light_tile_stride = 256; // has to match shader
    size_t tile_light_indices_size = light_tile_stride * util::divCeil(screen_extent.width, 16u) *
                                     util::divCeil(screen_extent.height, 16u);
    mTileLightIndicesBuffers.create(globals::MaxFramesInFlight, [&] {
        auto &&buf = Buffer::create(
                mContext->allocator(),
                {
                    .size = tile_light_indices_size * sizeof(glm::uint),
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer,
                }
        );
        util::setDebugName(device, *buf.buffer, "light_tile_indices");
        return buf;
    });

    mInstanceTransformUpdates.create(globals::MaxFramesInFlight, [&] {
        return Buffer{}; // initially empty; will be allocated on demand
    });
}

void RenderSystem::updateInstanceTransforms(const scene::GpuData &gpu_scene_data, std::span<const glm::mat4> updated_transforms) {
    if (updated_transforms.empty())
        return;

    vk::CommandBuffer &cmd = mPerFrameObjects.get().earlyGraphicsCommands;
    util::ScopedCommandLabel dbg_cmd_label_region(cmd, "Instance Transform Update");

    Buffer &staging_buffer = mInstanceTransformUpdates.get();
    const size_t required_size = updated_transforms.size() * sizeof(InstanceBlock);

    if (!staging_buffer || staging_buffer.size < required_size)
        staging_buffer = Buffer::create(
                mContext->allocator(),
                {
                    .size = required_size,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc,
                    .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
                    .device = vma::MemoryUsage::eAuto,
                    .requiredProperties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                }
        );

    std::memcpy(staging_buffer.persistentMapping, updated_transforms.data(), required_size);

    vk::DeviceSize dstOffset = gpu_scene_data.instances.size - updated_transforms.size() * sizeof(glm::mat4);
    vk::BufferCopy copy_region{0, dstOffset, required_size};
    gpu_scene_data.instances.barrier(cmd, BufferResourceAccess::TransferWrite);
    cmd.copyBuffer(staging_buffer, gpu_scene_data.instances, 1, &copy_region);
}

void RenderSystem::draw(const RenderData &rd) {
    const auto &frame_objects = mPerFrameObjects.get();
    const auto &desc_alloc = frame_objects.descriptorAllocator;
    const auto &buf_alloc = frame_objects.transientBufferAllocator;
    const auto &swapchain = mContext->swapchain();

    const auto &cmd_buf_compute = rd.settings.rendering.asyncCompute ? frame_objects.asyncComputeCommands
                                                                     : frame_objects.nonAsyncComputeCommands;

    auto time_record_start = std::chrono::high_resolution_clock::now();

    // Framebuffer needs to be synced to swapchain, so get it explicitly
    Framebuffer &swapchain_fb = mSwapchainFramebuffers.get(swapchain.activeImageIndex());

    // Early graphics
    {
        const auto &cmd_buf = frame_objects.earlyGraphicsCommands;
        util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Early Graphics");

        dbg_cmd_label_region.swap("Depth PrePass");

        // Depth pre-pass
        mDepthPrePassRenderer->enableCulling = rd.settings.rendering.enableFrustumCulling;
        mDepthPrePassRenderer->pauseCulling = rd.settings.rendering.pauseFrustumCulling;
        mDepthPrePassRenderer->execute(
                mContext->device(), desc_alloc, buf_alloc, cmd_buf, mHdrFramebuffer, mComputeDepthCopyImage, rd.camera,
                rd.gltfScene, *mFrustumCuller
        );

        dbg_cmd_label_region.swap("Blob System Update");
        rd.blobSystem.update(mContext->allocator(), mContext->device(), cmd_buf);
    }

    // Async compute
    {
        const auto &cmd_buf_early_graphics = frame_objects.earlyGraphicsCommands;
        cmd_buf_compute.begin(vk::CommandBufferBeginInfo{});
        util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf_compute, "Async Compute");

        if (rd.settings.ssao.update) {
            dbg_cmd_label_region.swap("SSAO Pass");

            // Transfer graphics -> compute queue
            // The compute queue doesn't allow issuing a barrier "from" a graphics stage. So do it prematurely.
            if (rd.settings.rendering.asyncCompute) {
                mComputeDepthCopyImage.barrier(cmd_buf_early_graphics, ImageResourceAccess::ComputeShaderStageOnly);
                mSsaoResultImage.barrier(cmd_buf_early_graphics, ImageResourceAccess::ComputeShaderStageOnly);
            }

            mSSAORenderer->radius = rd.settings.ssao.radius;
            mSSAORenderer->exponent = rd.settings.ssao.exponent;
            mSSAORenderer->bias = rd.settings.ssao.bias;
            mSSAORenderer->filterSharpness = rd.settings.ssao.filterSharpness;
            mSSAORenderer->execute(
                    mContext->device(), desc_alloc, cmd_buf_compute, rd.camera.projectionMatrix(),
                    rd.camera.nearPlane(), mComputeDepthCopyImage, mSsaoIntermediaryImage, mSsaoResultImage
            );
        }

        dbg_cmd_label_region.swap("Light Pass");
        auto &tile_light_indices_buffer = mTileLightIndicesBuffers.next();
        if (rd.settings.rendering.asyncCompute) {
            tile_light_indices_buffer.barrier(cmd_buf_early_graphics, BufferResourceAccess::ComputeShaderStageOnly);
        }
        mLightRenderer->lightRangeFactor = rd.settings.rendering.lightRangeFactor;
        mLightRenderer->execute(
                mContext->device(), desc_alloc, cmd_buf_compute, rd.gltfScene, rd.camera.projectionMatrix(),
                rd.camera.viewMatrix(), rd.camera.nearPlane(), mComputeDepthCopyImage, tile_light_indices_buffer
        );
    }

    // Independent Graphics (Don't need swapchain)
    {
        const auto &cmd_buf = frame_objects.independentGraphicsCommands;
        cmd_buf.begin(vk::CommandBufferBeginInfo{});

        // Shadow pass
        if (rd.settings.shadowCascade.update) {
            util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Shadow Pass");
            const ShadowCaster *inner = nullptr;
            for (auto &caster: rd.sunShadowCasterCascade.cascades()) {
                // Objects contained in the inner cascade are culled form the outer cascade
                mShadowRenderer->execute(
                        mContext->device(), desc_alloc, buf_alloc, cmd_buf, rd.gltfScene, *mFrustumCuller, caster, inner
                );
                inner = &caster;
            }
        }

        util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Blob Pass");
        mBlobRenderer->compute(mContext->device(), cmd_buf, rd.blobSystem, rd.timestamp);
    }

    // Main Graphics
    {
        const auto &cmd_buf = frame_objects.mainGraphicsCommands;
        cmd_buf.begin(vk::CommandBufferBeginInfo{});
        // command buffer begin already called
        util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Main Graphics");

        // Main render pass
        dbg_cmd_label_region.swap("PBR Scene Pass");
        mPbrSceneRenderer->enableCulling = rd.settings.rendering.enableFrustumCulling;
        mPbrSceneRenderer->pauseCulling = rd.settings.rendering.pauseFrustumCulling;
        mPbrSceneRenderer->execute(
                mContext->device(), desc_alloc, buf_alloc, cmd_buf, mHdrFramebuffer, rd.camera, rd.gltfScene,
                *mFrustumCuller, rd.sunLight, rd.sunShadowCasterCascade.cascades(), mSsaoResultImage,
                mTileLightIndicesBuffers.get(), rd.settings
        );

        // Skybox render pass (render late to reduce overdraw)
        dbg_cmd_label_region.swap("Skybox Pass");
        mSkyboxRenderer->execute(
                mContext->device(), desc_alloc, cmd_buf, mHdrFramebuffer, rd.camera, rd.skybox,
                rd.settings.sky.exposure, rd.settings.sky.tint
        );

        // Blob render pass
        if (rd.settings.animation.renderBlob) {
            dbg_cmd_label_region.swap("Blob Pass");

            storeHdrColorImage(cmd_buf);

            mBlobRenderer->draw(mContext->device(), cmd_buf, mHdrFramebuffer, mStoredHdrColorImage, rd.camera, rd.sunLight, rd.settings.rendering.ambient, rd.blobSystem);
        }

        // MSAA Resolve
        bool msaa = mHdrColorAttachment.imageInfo().samples != vk::SampleCountFlagBits::e1;
        if (msaa) {
            resolveHdrColorImage(cmd_buf);
        }
        const ImageWithView &resolved_hdr_color_image = msaa ? mHdrColorResolveImage : mHdrColorAttachment;


        // Fog render pass
        dbg_cmd_label_region.swap("Fog Pass");
        mFogRenderer->samples = rd.settings.fog.samples;
        mFogRenderer->targetStepContribution = rd.settings.fog.targetStepContribution;
        mFogRenderer->density = rd.settings.fog.density;
        mFogRenderer->g = rd.settings.fog.g;
        mFogRenderer->heightFalloff = rd.settings.fog.heightFalloff;
        mFogRenderer->execute(
                mContext->device(), desc_alloc, buf_alloc, cmd_buf, mHdrFramebuffer.depthAttachment,
                resolved_hdr_color_image, rd.sunLight, rd.settings.rendering.ambient, rd.settings.fog.color,
                rd.sunShadowCasterCascade.cascades(), rd.camera.viewMatrix(), rd.camera.projectionMatrix(),
                rd.camera.nearPlane(), mFrameNumber
        );

        // Bloom pass
        {
            dbg_cmd_label_region.swap("Bloom Pass");
            mBloomRenderer->threshold = rd.settings.bloom.threshold;
            mBloomRenderer->knee = rd.settings.bloom.knee;

            for (int i = 0; i < mBloomRenderer->factors.size(); i++)
                mBloomRenderer->factors[i] = rd.settings.bloom.factors[i];
            mBloomRenderer->execute(mContext->device(), desc_alloc, cmd_buf, resolved_hdr_color_image);
        }

        // Post-processing pass
        dbg_cmd_label_region.swap("Post-Process Pass");

        mFinalizeRenderer->execute(
                mContext->device(), desc_alloc, cmd_buf, resolved_hdr_color_image, swapchain_fb.colorAttachments[0],
                mBloomRenderer->result(), rd.settings.agx
        );

        // ImGui render pass
        dbg_cmd_label_region.swap("ImGUI Pass");
        {
            swapchain_fb.colorAttachments[0].image().barrier(
                    cmd_buf, ImageResourceAccess::ColorAttachmentLoad, ImageResourceAccess::ColorAttachmentWrite
            );
            swapchain_fb.depthAttachment.image().barrier(
                    cmd_buf, ImageResourceAccess::DepthAttachmentEarlyOps, ImageResourceAccess::DepthAttachmentLateOps
            );
            // temporarily change view to linear format to fix an ImGui issue.
            Framebuffer imgui_fb = swapchain_fb;
            imgui_fb.colorAttachments[0] =
                    ImageViewPair(swapchain_fb.colorAttachments[0].image(), swapchain.colorViewLinear());
            cmd_buf.beginRendering(imgui_fb.renderingInfo({}));
            mImguiBackend->render(cmd_buf);
            cmd_buf.endRendering();
        }

        swapchain_fb.colorAttachments[0].image().barrier(cmd_buf, ImageResourceAccess::PresentSrc);
    }

    auto time_record_end = std::chrono::high_resolution_clock::now();
    mTimings.record = std::chrono::duration<double, std::milli>(time_record_end - time_record_start).count();

    // Submit early graphics work
    {
        frame_objects.earlyGraphicsCommands.end();
        std::array signal = {*frame_objects.earlyGraphicsFinishedSemaphore};
        auto submit_info = vk::SubmitInfo().setCommandBuffers(frame_objects.earlyGraphicsCommands);
        if (rd.settings.rendering.asyncCompute) {
            submit_info.setSignalSemaphores(signal);
        }
        mContext->mainQueue->submit(submit_info);
    }

    // Submit async compute
    {
        cmd_buf_compute.end();
        vk::PipelineStageFlags wait_mask = vk::PipelineStageFlagBits::eComputeShader;
        std::array wait = {*frame_objects.earlyGraphicsFinishedSemaphore};
        std::array signal = {*frame_objects.asyncComputeFinishedSemaphore};
        auto submit_info = vk::SubmitInfo().setCommandBuffers(cmd_buf_compute);
        if (rd.settings.rendering.asyncCompute) {
            submit_info.setWaitSemaphores(wait).setWaitDstStageMask(wait_mask).setSignalSemaphores(signal);
            mContext->computeQueue->submit(submit_info);
        } else {
            mContext->mainQueue->submit(submit_info);
        }
    }

    // Sumit independent graphics
    {
        frame_objects.independentGraphicsCommands.end();
        // Don't need to wait because submission is on the same queue as early graphics
        // For the same reason it doesn't need to signal for the main graphics
        mContext->mainQueue->submit(vk::SubmitInfo().setCommandBuffers(frame_objects.independentGraphicsCommands));
    }

    auto time_submit_end = std::chrono::high_resolution_clock::now();
    mTimings.submit = std::chrono::duration<double, std::milli>(time_submit_end - time_record_end).count();
}

void RenderSystem::advance(const Settings &settings) {
    auto &swapchain = mContext->swapchain();
    auto &frame_objects = mPerFrameObjects.next();

    auto time_fence_start = std::chrono::high_resolution_clock::now();
    mBeginTime = time_fence_start;

    while (mContext->device().waitForFences(*frame_objects.inFlightFence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }

    auto time_fence_end = std::chrono::high_resolution_clock::now();
    mTimings.fence = std::chrono::duration<double, std::milli>(time_fence_end - time_fence_start).count();

    // It would be better to do this later in the frame to give the host more time to release the image
    if (!swapchain.advance(*frame_objects.imageAvailableSemaphore)) {
        recreate(settings);
    }

    auto time_advance_end = std::chrono::high_resolution_clock::now();
    mTimings.advance = std::chrono::duration<double, std::milli>(time_advance_end - time_fence_end).count();

    mInstanceTransformUpdates.next();
}

void RenderSystem::begin() {
    mPerFrameObjects.get().reset(mContext->device());

    // the main graphics commands are used elsewhere, so begin them early
    mPerFrameObjects.get().earlyGraphicsCommands.begin(vk::CommandBufferBeginInfo{});
}

void RenderSystem::submit(const Settings &settings) {
    const auto &frame_objects = mPerFrameObjects.get();
    // These must correspond to the active swapchain image index, because the semaphore only becomes unsignaled
    // once the swapchain image is released (and acquired)
    // https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
    const auto &render_finished_semaphore = mRenderFinishedSemaphore.get(mContext->swapchain().activeImageIndex());

    auto time_submit_start = std::chrono::high_resolution_clock::now();

    frame_objects.mainGraphicsCommands.end();
    util::static_vector<vk::Semaphore, 2> wait_semaphores = {*frame_objects.imageAvailableSemaphore};
    util::static_vector<vk::PipelineStageFlags, 2> wait_masks = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    if (settings.rendering.asyncCompute) {
        wait_semaphores.push_back(*frame_objects.asyncComputeFinishedSemaphore);
        wait_masks.push_back(vk::PipelineStageFlagBits::eComputeShader); // I'm really not sure about this one
    }
    auto submit_info = vk::SubmitInfo()
                               .setCommandBuffers(frame_objects.mainGraphicsCommands)
                               .setWaitSemaphores(wait_semaphores)
                               .setWaitDstStageMask(wait_masks)
                               .setSignalSemaphores(*render_finished_semaphore);

    mContext->mainQueue->submit({submit_info}, *frame_objects.inFlightFence);

    auto time_submit_end = std::chrono::high_resolution_clock::now();
    mTimings.submit += std::chrono::duration<double, std::milli>(time_submit_end - time_submit_start).count();

    if (!mContext->swapchain().present(
                mContext->presentQueue, vk::PresentInfoKHR().setWaitSemaphores(*render_finished_semaphore)
        )) {
        recreate(settings);
    }

    auto time_present_end = std::chrono::high_resolution_clock::now();
    mTimings.present = std::chrono::duration<double, std::milli>(time_present_end - time_submit_end).count();

    mTimings.total = std::chrono::duration<double, std::milli>(time_present_end - mBeginTime).count();

    mFrameNumber++;
}

void RenderSystem::resolveHdrColorImage(const vk::CommandBuffer &cmd_buf) const {
    util::ScopedCommandLabel dbg_cmd_label_region = {cmd_buf, "Resolve HDR Color Image"};
    mHdrColorAttachment.barrier(cmd_buf, ImageResourceAccess::TransferRead);
    mHdrColorResolveImage.barrier(cmd_buf, ImageResourceAccess::TransferWrite);
    auto resolve_region = vk::ImageResolve2{
        .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
        .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
        .extent = mHdrColorAttachment.imageInfo().extents()
    };
    cmd_buf.resolveImage2({
        .srcImage = mHdrColorAttachment,
        .srcImageLayout = ImageResourceAccess::TransferRead.layout,
        .dstImage = mHdrColorResolveImage,
        .dstImageLayout = ImageResourceAccess::TransferWrite.layout,
        .regionCount = 1,
        .pRegions = &resolve_region,
    });
}

void RenderSystem::storeHdrColorImage(const vk::CommandBuffer &cmd_buf) const {
    bool msaa = mHdrColorAttachment.imageInfo().samples != vk::SampleCountFlagBits::e1;
    if (msaa) {
        resolveHdrColorImage(cmd_buf);
    }

    util::ScopedCommandLabel dbg_cmd_label_region = {cmd_buf, "Blit HDR Color Image"};

    const ImageBase &hdr_color_image = msaa ? mHdrColorResolveImage : mHdrColorAttachment;
    hdr_color_image.barrier(cmd_buf, ImageResourceAccess::TransferRead);

    mStoredHdrColorImage.barrier(cmd_buf, ImageResourceAccess::TransferWrite);
    auto region = vk::ImageBlit2{
        .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
        .srcOffsets =
                std::array{
                    vk::Offset3D{},
                    vk::Offset3D{
                        static_cast<int32_t>(hdr_color_image.info.width), static_cast<int32_t>(hdr_color_image.info.height), 1
                    }
                },
        .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .layerCount = 1},
        .dstOffsets =
                std::array{
                    vk::Offset3D{},
                    vk::Offset3D{
                        static_cast<int32_t>(mStoredHdrColorImage.imageInfo().width),
                        static_cast<int32_t>(mStoredHdrColorImage.imageInfo().height), 1
                    }
                },
    };
    cmd_buf.blitImage2(vk::BlitImageInfo2{
        .srcImage = hdr_color_image,
        .srcImageLayout = ImageResourceAccess::TransferRead.layout,
        .dstImage = mStoredHdrColorImage,
        .dstImageLayout = ImageResourceAccess::TransferWrite.layout,
        .regionCount = 1,
        .pRegions = &region,
        .filter = vk::Filter::eLinear,
    });
}
