
#include "Cubemap.h"

#include <cstring>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "../backend/DeviceQueue.h"
#include "../backend/StagingBuffer.h"

Cubemap::Cubemap(
        const vma::Allocator &allocator,
        const vk::Device &device,
        const DeviceQueue &transferQueue,
        const DeviceQueue &graphicsQueue,
        const std::array<std::string, FACES_COUNT> &skyboxImageFilenames
)
    : allocator{allocator}, device{device} {
    vk::UniqueCommandPool graphicsCommandPool = device.createCommandPoolUnique(
            {.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = graphicsQueue.family}
    );

    vk::UniqueCommandPool transferCommandPool = device.createCommandPoolUnique(
            {.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = transferQueue.family}
    );

    vk::CommandBuffer graphicsCommandBuffer = device.allocateCommandBuffers(
        {.commandPool = *graphicsCommandPool, .commandBufferCount = 1}
    ).at(0);

    graphicsCommandBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    std::array<PlainImageData, FACES_COUNT> plainImages{};

    for (int i{0}; i < FACES_COUNT; ++i)
        plainImages[i] = PlainImageData::create(FORMAT, skyboxImageFilenames[i]);

    std::vector<unsigned char> pixelData = getPixelData(plainImages);

    StagingBuffer stagingBuffer = {allocator, device, *transferCommandPool};
    vk::Buffer stagedBuffer = stagingBuffer.stage(pixelData);

    ImageCreateInfo imageCreateInfo = ImageCreateInfo::from(plainImages[0]);
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eSampled;
    imageCreateInfo.mip_levels = 1;
    imageCreateInfo.array_layers = FACES_COUNT;
    imageCreateInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;


    image = Image::create(stagingBuffer.allocator(), imageCreateInfo);
    image.load(stagingBuffer.commands(), 0, {}, stagedBuffer);
    image.transfer(stagingBuffer.commands(), graphicsCommandBuffer, transferQueue, graphicsQueue);
    image.barrier(graphicsCommandBuffer, ImageResourceAccess::FragmentShaderReadOptimal);

    graphicsCommandBuffer.end();

    vk::UniqueFence graphicsQueueFence = device.createFenceUnique({});
    vk::UniqueSemaphore imageTransferSemaphore = device.createSemaphoreUnique({});

    stagingBuffer.submit(transferQueue, vk::SubmitInfo().setSignalSemaphores(*imageTransferSemaphore));

    vk::PipelineStageFlags semaphoreStageMask = vk::PipelineStageFlagBits::eTopOfPipe;

    graphicsQueue.queue.submit(
            {vk::SubmitInfo()
                     .setWaitSemaphores(*imageTransferSemaphore)
                     .setCommandBuffers(graphicsCommandBuffer)
                     .setWaitDstStageMask(semaphoreStageMask)},
            *graphicsQueueFence
    );

    device.waitForFences(*graphicsQueueFence, true, UINT64_MAX);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = image.image;
    viewInfo.viewType = vk::ImageViewType::eCube;
    viewInfo.format = FORMAT;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = FACES_COUNT;

    view = device.createImageViewUnique(viewInfo);
}

std::vector<unsigned char> Cubemap::getPixelData(std::array<PlainImageData, FACES_COUNT> plainImages) {
    vk::Format format = plainImages[0].format;
    
    const vk::DeviceSize faceSize = plainImages[0].pixels.size();
    for (int i{1}; i < FACES_COUNT; ++i)
        assert(plainImages[i].pixels.size() == faceSize && "All faces of the skybox must have the same size");

    std::vector<unsigned char> pixelData(static_cast<std::size_t>(faceSize) * FACES_COUNT);

    std::size_t offset = 0;
    for (const auto &image: plainImages) {
        std::copy(image.pixels.begin(), image.pixels.end(), pixelData.begin() + offset);
        offset += image.pixels.size();
    }

    return pixelData;
}
