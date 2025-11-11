#include "BlobModel.h"

#include <memory>
#include <tuple>

BlobModel::BlobModel(const vma::Allocator& allocator) : allocator{ allocator } {
    createVertexStagingBuffer();
    createVertexBuffer();
}

BlobModel::~BlobModel() {
    vmaDestroyBuffer(allocator, vertexBuffer, vertexAlloc);
    vmaDestroyBuffer(allocator, vertexStagingBuffer, vertexStagingAlloc);
}

void BlobModel::updateVertices(vk::CommandBuffer commandBuffer,
    const std::vector<VertexData> &vertices
) {
    vertexCount = static_cast<uint32_t>(vertices.size());

    assert(vertices.size() <= MAX_VERTICES && "Temp: Insufficient buffer capacity");

    VkDeviceSize dataSize = sizeof(VertexData) * vertexCount;

    std::memcpy(stagingData, vertices.data(), static_cast<size_t>(dataSize));

    vk::BufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = dataSize;
    commandBuffer.copyBuffer(vertexStagingBuffer, vertexBuffer, 1, &copy);

    vk::BufferMemoryBarrier barrier{};
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead;
    barrier.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    barrier.buffer = vertexBuffer;
    barrier.offset = 0;
    barrier.size = dataSize;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eVertexInput, {}, {}, barrier, {});
}

void BlobModel::bind(vk::CommandBuffer commandBuffer) const {
    vk::Buffer buffers[] = {vertexBuffer};
    vk::DeviceSize offsets[] = {0};
    commandBuffer.bindVertexBuffers(0, 1, buffers, offsets);
}

void BlobModel::draw(vk::CommandBuffer commandBuffer) const {
    commandBuffer.draw(vertexCount, 0, 0, 0);
}

void BlobModel::createVertexStagingBuffer() {
    vk::BufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.size = VERTEX_BUFFER_SIZE;
    bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

    vma::AllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                            vma::AllocationCreateFlagBits::eMapped;
    allocCreateInfo.usage = vma::MemoryUsage::eAuto;
    allocCreateInfo.requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;

    vma::AllocationInfo stagingInfo{};

    std::tie(vertexStagingBuffer, vertexStagingAlloc) = allocator.createBuffer(bufferCreateInfo, allocCreateInfo, stagingInfo);

    stagingData = stagingInfo.pMappedData;
}

void BlobModel::createVertexBuffer() {
    vk::BufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.size = VERTEX_BUFFER_SIZE;
    bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;

    vma::AllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = vma::MemoryUsage::eAutoPreferDevice;

    std::tie(vertexBuffer, vertexAlloc) = allocator.createBuffer(bufferCreateInfo, allocCreateInfo);
}