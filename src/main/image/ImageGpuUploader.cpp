#include "ImageGpuUploader.h"

#include "../debug/Annotation.h"
#include "../util/globals.h"

ImageGpuUploader::ImageGpuUploader(
        const vma::Allocator &allocator, const vk::Device &device, const DeviceQueue &graphicsQueue, const DeviceQueue &transferQueue
)
    : mAllocator(allocator),
      mDevice(device),
      mGraphicsQueue(graphicsQueue),
      mTransferQueue(transferQueue),
      mTransferCommandPool(device.createCommandPoolUnique(
              {.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
               .queueFamilyIndex = transferQueue}
      )) {

    mGraphicsCommandPool = device.createCommandPoolUnique(
            {.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
             .queueFamilyIndex = graphicsQueue}
    );

    mActiveInitCmd = std::move(device.allocateCommandBuffersUnique({.commandPool = *mGraphicsCommandPool,
                                                                    .level = vk::CommandBufferLevel::ePrimary,
                                                                    .commandBufferCount = 1})
                                       .front());
    util::setDebugName(mDevice, *mActiveInitCmd, "image_uploader_init_cmds");

    mFrameResources.create(util::MaxFramesInFlight, [&] {
        auto cmds = device.allocateCommandBuffersUnique(
                {.commandPool = *mGraphicsCommandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 2}
        );
        util::setDebugName(mDevice, *cmds[0], "image_uploader_pre_cmds");
        util::setDebugName(mDevice, *cmds[1], "image_uploader_post_cmds");

        return FrameResources{
            .preCmd = std::move(cmds[0]),
            .postCmd = std::move(cmds[1]),
            .graphicsRelease = device.createSemaphoreUnique({}),
            .transferComplete = device.createSemaphoreUnique({}),
            .retiredInitCmds = {},
            .staging = StagingBuffer(allocator, device, *mTransferCommandPool)
        };
    });

    mActiveInitCmd->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
}

void ImageGpuUploader::initialize(ImageBase *target, ImageResourceAccess initialState) {
    std::lock_guard lock(mMutex);

    ImageResourceAccess transferDst = {
        vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal
    };
    target->barrier(*mActiveInitCmd, transferDst);

    vk::ClearColorValue color = {std::array<float, 4>{1.0f, 0.0f, 1.0f, 1.0f}};
    vk::ImageSubresourceRange range = target->info.resourceRange();
    mActiveInitCmd->clearColorImage(*target, vk::ImageLayout::eTransferDstOptimal, &color, 1, &range);

    target->barrier(*mActiveInitCmd, initialState);

    mActiveRecordedInitCmds++;
}

void ImageGpuUploader::flushInit(bool wait) {
    if (mActiveRecordedInitCmds == 0) {
        return;
    }

    std::lock_guard lock(mMutex);
    mActiveInitCmd->end();

    vk::SubmitInfo submit = {};
    submit.setCommandBuffers(*mActiveInitCmd);


    if (wait) {
        auto fence = mDevice.createFenceUnique({});
        mGraphicsQueue.queue.submit(submit, *fence);
        while (mDevice.waitForFences(*fence, true, UINT64_MAX) == vk::Result::eTimeout) {
        }
        mFrameResources.get().retiredInitCmds.clear();
        mActiveInitCmd->reset();
        mActiveInitCmd->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    } else {
        mGraphicsQueue.queue.submit(submit);
        mFrameResources.get().retiredInitCmds.emplace_back(std::move(mActiveInitCmd));
        mActiveInitCmd = std::move(mDevice.allocateCommandBuffersUnique({.commandPool = *mGraphicsCommandPool,
                                                                         .level = vk::CommandBufferLevel::ePrimary,
                                                                         .commandBufferCount = 1})
                                           .front());
        util::setDebugName(mDevice, *mActiveInitCmd, "image_uploader_init_cmds");
        mActiveInitCmd->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    }
    mActiveRecordedInitCmds = 0;
}

void ImageGpuUploader::uploadExplicit(ImageBase *target, std::span<const ImageCopyCmd> cmds, UploadJobConfig config) {
    std::lock_guard lock(mMutex);

    size_t totalSize = 0;
    for (const auto &cmd: cmds)
        totalSize += cmd.source->size();

    auto [stagingBuf, mappedPtr] = mFrameResources.get().staging.stage(totalSize);
    auto *bytePtr = static_cast<std::byte *>(mappedPtr);

    std::vector<vk::BufferImageCopy> regions;
    size_t offset = 0;

    for (const auto &cmd: cmds) {
        std::memcpy(bytePtr + offset, cmd.source->data.get(), cmd.source->size());

        vk::BufferImageCopy copy = {};
        copy.bufferOffset = offset;
        copy.imageSubresource.aspectMask = target->info.aspects;
        copy.imageSubresource.mipLevel = cmd.mipLevel;
        copy.imageSubresource.baseArrayLayer = cmd.arrayLayer;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {cmd.source->width, cmd.source->height, 1};

        regions.push_back(copy);
        offset += cmd.source->size();
    }

    mQueue.push_back({target, stagingBuf, std::move(regions), config});
}

void ImageGpuUploader::upload(ImageBase *target, const ImageData &data, UploadJobConfig config) {
    ImageCopyCmd cmd = {&data, 0, 0};
    uploadExplicit(target, std::span(&cmd, 1), config);
}

void ImageGpuUploader::uploadLayered(ImageBase *target, std::span<const ImageData> layers, UploadJobConfig config) {
    std::vector<ImageCopyCmd> cmds;
    cmds.reserve(layers.size());
    for (uint32_t i = 0; i < layers.size(); ++i) {
        cmds.push_back({&layers[i], 0, i});
    }
    uploadExplicit(target, cmds, config);
}

void ImageGpuUploader::apply(
        std::span<const vk::Semaphore> waitSemaphores,
        std::span<const vk::PipelineStageFlags> waitStages,
        std::span<const vk::Semaphore> signalSemaphores,
        vk::Fence fence
) {
    flushInit();

    std::lock_guard lock(mMutex);

    auto &[preCmd, postCmd, graphicsRelease, transferComplete, retiredInitCmds, staging] = mFrameResources.get();

    preCmd->reset();
    postCmd->reset();
    preCmd->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    postCmd->begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    for (const auto &job: mQueue) {
        ImageResourceAccess final_state = job.config.finalState.has_value() ? job.config.finalState.value()
                                                                            : job.target->getBarrierState();

        // 1. Graphics: Final -> TransferDst + Release
        ImageResourceAccess transferDst = {
            vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eTransferDstOptimal
        };
        job.target->barrier(*preCmd, transferDst);
        job.target->transfer(*preCmd, staging.commands(), mGraphicsQueue, mTransferQueue);

        // 2. Transfer: Copy + Release
        // Note: We use copyBufferToImage manually because we pre-calculated regions in PendingUpload
        // This avoids calling job.target->load() repeatedly.
        // But we must assume 'target' is in TransferDstOptimal (which it is, from the Acquire implicit in transfer())
        // Wait, 'transfer' on staging cmd records an Acquire barrier.

        // Manual copy command since we have the list of regions
        staging.commands().copyBufferToImage(
                job.stagingBuffer, static_cast<vk::Image>(*job.target), vk::ImageLayout::eTransferDstOptimal, job.regions
        );

        job.target->transfer(staging.commands(), *postCmd, mTransferQueue, mGraphicsQueue);

        // 3. Graphics: Mips + Final
        if (job.config.generateMipmaps) {
            job.target->generateMipmaps(*postCmd);
        }
        job.target->barrier(*postCmd, final_state);
    }
    mQueue.clear();

    preCmd->end();
    postCmd->end();

    // Submission Logic
    vk::Semaphore gfxReleaseSem = *graphicsRelease;
    vk::Semaphore transferDoneSem = *transferComplete;

    // Submit 1 (Gfx Pre)
    {
        vk::SubmitInfo si = {};
        si.setCommandBuffers(*preCmd);
        si.setWaitSemaphores(waitSemaphores);
        si.setWaitDstStageMask(waitStages);
        si.setSignalSemaphores(gfxReleaseSem);
        mGraphicsQueue.queue.submit(si);
    }

    // Submit 2 (Transfer)
    {
        vk::SubmitInfo si = {};
        std::array<vk::Semaphore, 1> w = {gfxReleaseSem};
        std::array<vk::PipelineStageFlags, 1> s = {vk::PipelineStageFlagBits::eTransfer};
        std::array<vk::Semaphore, 1> sig = {transferDoneSem};
        si.setWaitSemaphores(w);
        si.setWaitDstStageMask(s);
        si.setSignalSemaphores(sig);
        staging.submitUnsynchronized(mTransferQueue.queue, si);
    }

    // Submit 3 (Gfx Post)
    {
        vk::SubmitInfo si = {};
        si.setCommandBuffers(*postCmd);
        std::array<vk::Semaphore, 1> w = {transferDoneSem};
        std::array<vk::PipelineStageFlags, 1> s = {vk::PipelineStageFlagBits::eTransfer};
        si.setWaitSemaphores(w);
        si.setWaitDstStageMask(s);
        si.setSignalSemaphores(signalSemaphores);
        mGraphicsQueue.queue.submit(si, fence);
    }

    auto &nextFrameRes = mFrameResources.next();
    nextFrameRes.retiredInitCmds.clear();
    nextFrameRes.staging.beginUnsynchronized();
}

size_t ImageGpuUploader::getPendingUploadCount() const {
    // Lock needed if called from other threads, though usually main thread only
    return mQueue.size();
}
