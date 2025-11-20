
#include "Cubemap.h"

#include <cstring>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include "../backend/DeviceQueue.h"
#include "../backend/StagingBuffer.h"

// TODO: Use "StagingBuffer" and "Image" abstractions if possible

Cubemap::Cubemap(
        const vma::Allocator &allocator,
        const vk::Device &device,
        const DeviceQueue &queue,
        const std::array<std::string, FACES_COUNT> &skyboxImageFilenames
)
    : allocator{allocator}, device{device} {
    vk::CommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eTransient;
    commandPoolCreateInfo.queueFamilyIndex = queue.family;

    vk::CommandPool commandPool = device.createCommandPool(commandPoolCreateInfo);

    vk::CommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.commandBufferCount = 1;

    vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(commandBufferAllocateInfo).at(0);

    vk::CommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(commandBufferBeginInfo);

    vk::Format format = vk::Format::eR8G8B8A8Srgb;
    std::array<PlainImageData, FACES_COUNT> plainImages{};

    for (int i{0}; i < FACES_COUNT; ++i)
        plainImages[i] = PlainImageData::create(format, skyboxImageFilenames[i]);

    const vk::DeviceSize faceSize = plainImages[0].pixels.size();
    for (int i{1}; i < FACES_COUNT; ++i)
        assert(plainImages[i].pixels.size() == faceSize && "All faces of the skybox must have the same size");

    vk::Buffer stagingBuffer;
    vma::Allocation stagingAlloc;

    vk::BufferCreateInfo stagingBufferCreateInfo{};
    stagingBufferCreateInfo.size = faceSize * FACES_COUNT;
    stagingBufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

    vma::AllocationCreateInfo stagingAllocCreateInfo{};
    stagingAllocCreateInfo.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                                   vma::AllocationCreateFlagBits::eMapped;
    stagingAllocCreateInfo.requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible |
                                           vk::MemoryPropertyFlagBits::eHostCoherent;
    stagingAllocCreateInfo.usage = vma::MemoryUsage::eAuto;

    vma::AllocationInfo stagingAllocInfo{};

    std::tie(stagingBuffer, stagingAlloc) =
            allocator.createBuffer(stagingBufferCreateInfo, stagingAllocCreateInfo, &stagingAllocInfo);

    void *stagingData = stagingAllocInfo.pMappedData;

    vk::DeviceSize offset = 0;
    for (int i{0}; i < FACES_COUNT; ++i) {
        std::memcpy(
                static_cast<char *>(stagingData) + offset, plainImages[i].pixels.data(), static_cast<std::size_t>(faceSize)
        );
        offset += faceSize;
    }

    vk::ImageCreateInfo imageCreateInfo{};
    imageCreateInfo.imageType = vk::ImageType::e2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent = vk::Extent3D{plainImages[0].width, plainImages[0].height, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = FACES_COUNT;
    imageCreateInfo.samples = vk::SampleCountFlagBits::e1;
    imageCreateInfo.tiling = vk::ImageTiling::eOptimal;
    imageCreateInfo.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imageCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageCreateInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;

    vma::AllocationCreateInfo imageAllocCreateInfo{};
    imageAllocCreateInfo.usage = vma::MemoryUsage::eAuto;
    imageAllocCreateInfo.requiredFlags = vk::MemoryPropertyFlagBits::eDeviceLocal;

    vma::AllocationInfo imageAllocInfo{};

    std::tie(image, imageAlloc) = allocator.createImage(imageCreateInfo, imageAllocCreateInfo, &imageAllocInfo);

    transitionImageLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = FACES_COUNT;
    region.imageOffset = vk::Offset3D{0, 0, 0};
    region.imageExtent = vk::Extent3D{plainImages[0].width, plainImages[0].height, 1};

    commandBuffer.copyBufferToImage(stagingBuffer, image, vk::ImageLayout::eTransferDstOptimal, region);

    transitionImageLayout(commandBuffer, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::eCube;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = FACES_COUNT;

    view = device.createImageViewUnique(viewInfo);

    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vk::FenceCreateInfo fenceCreateInfo{};
    vk::Fence fence = device.createFence(fenceCreateInfo);

    queue.queue.submit({submitInfo}, fence);

    while (device.waitForFences(fence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }

    device.destroyFence(fence);
    allocator.destroyBuffer(stagingBuffer, stagingAlloc);
    device.destroyCommandPool(commandPool);
}

Cubemap::~Cubemap() { allocator.destroyImage(image, imageAlloc); }

void Cubemap::transitionImageLayout(
        const vk::CommandBuffer &commandBuffer, vk::ImageLayout oldLayout, vk::ImageLayout newLayout
) const {
    vk::ImageMemoryBarrier barrier{};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = FACES_COUNT;

    vk::PipelineStageFlags srcStage;
    vk::PipelineStageFlags dstStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        srcStage = vk::PipelineStageFlagBits::eTopOfPipe;
        dstStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        srcStage = vk::PipelineStageFlagBits::eTransfer;
        dstStage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::runtime_error("Unsupported image layout transition");
    }

    commandBuffer.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr, barrier);
}
