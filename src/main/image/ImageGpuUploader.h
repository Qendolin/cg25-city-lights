#pragma once
#include <vulkan/vulkan.hpp>
#include <mutex>
#include <vector>

#include "../backend/Image.h"
#include "ImageTypes.h"
#include "../backend/StagingBuffer.h"
#include "../backend/DeviceQueue.h"
#include "../util/PerFrame.h" // Assuming this exists per previous context

struct UploadJobConfig {
    bool generateMipmaps = false;
    std::optional<ImageResourceAccess> finalState = std::nullopt;
};

struct ImageCopyCmd {
    const ImageData* source;
    uint32_t mipLevel = 0;
    uint32_t arrayLayer = 0;
};

class ImageGpuUploader {
public:
    ImageGpuUploader(const vma::Allocator& allocator, const vk::Device& device,
                     const DeviceQueue& graphicsQueue, const DeviceQueue& transferQueue);

    // ------------------------------------------------
    // Phase 1: Initialization (Fire-and-Forget)
    // ------------------------------------------------
    void initialize(ImageBase* target, ImageResourceAccess initialState);

    // Submits the initialization buffer immediately and optionally waits.
    void flushInit(bool wait = false);

    // ------------------------------------------------
    // Phase 2: Upload Queueing
    // ------------------------------------------------
    void upload(ImageBase* target, const ImageData& data, UploadJobConfig config = {});
    void uploadLayered(ImageBase* target, std::span<const ImageData> layers, UploadJobConfig config = {});
    void uploadExplicit(ImageBase* target, std::span<const ImageCopyCmd> cmds, UploadJobConfig config = {});

    // ------------------------------------------------
    // Phase 3: Execution
    // ------------------------------------------------
    void apply(std::span<const vk::Semaphore> waitSemaphores = {},
               std::span<const vk::PipelineStageFlags> waitStages = {},
               std::span<const vk::Semaphore> signalSemaphores = {},
               vk::Fence fence = nullptr);

    [[nodiscard]] size_t getPendingUploadCount() const;

private:
    struct PendingUpload {
        ImageBase* target;
        vk::Buffer stagingBuffer;
        std::vector<vk::BufferImageCopy> regions;
        UploadJobConfig config;
    };

    struct FrameResources {
        vk::UniqueCommandBuffer preCmd;
        vk::UniqueCommandBuffer postCmd;
        vk::UniqueSemaphore graphicsRelease;
        vk::UniqueSemaphore transferComplete;
        std::vector<vk::UniqueCommandBuffer> retiredInitCmds;
        StagingBuffer staging;
    };

    vma::Allocator mAllocator;
    vk::Device mDevice;
    DeviceQueue mGraphicsQueue;
    DeviceQueue mTransferQueue;

    vk::UniqueCommandPool mTransferCommandPool;
    vk::UniqueCommandPool mGraphicsCommandPool;

    vk::UniqueCommandBuffer mActiveInitCmd;
    uint32_t mActiveRecordedInitCmds = 0;

    util::PerFrame<FrameResources> mFrameResources;

    std::mutex mMutex;
    std::vector<PendingUpload> mQueue;
};