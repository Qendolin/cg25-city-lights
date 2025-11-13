#include "Model.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <memory>
#include <tuple>
#include <algorithm>

namespace blob {

    Model::Model(const vma::Allocator &allocator) : allocator{allocator} {
        createVertexStagingBuffer();
        createVertexBuffer();
        modelMatrix = glm::translate(modelMatrix, {0.f, 1.f, 1.5f});
    }

    Model::~Model() {
        vmaDestroyBuffer(allocator, vertexBuffer, vertexAlloc);
        vmaDestroyBuffer(allocator, vertexStagingBuffer, vertexStagingAlloc);
    }

    void Model::pushVertices(vk::CommandBuffer commandBuffer) const {
        uint32_t vertexCount = static_cast<uint32_t>(std::min(vertices.size(), MAX_VERTICES));
        vk::DeviceSize dataSize = sizeof(VertexData) * vertexCount;

        std::memcpy(stagingData, vertices.data(), static_cast<std::size_t>(dataSize));

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

        commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput, {}, {}, barrier, {}
        );
    }

    void Model::createVertexStagingBuffer() {
        vk::BufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.size = VERTEX_BUFFER_SIZE;
        bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

        vma::AllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                                vma::AllocationCreateFlagBits::eMapped;
        allocCreateInfo.usage = vma::MemoryUsage::eAuto;
        allocCreateInfo.requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible |
                                        vk::MemoryPropertyFlagBits::eHostCoherent;

        vma::AllocationInfo stagingInfo{};

        std::tie(vertexStagingBuffer, vertexStagingAlloc) =
                allocator.createBuffer(bufferCreateInfo, allocCreateInfo, stagingInfo);

        stagingData = stagingInfo.pMappedData;
    }

    void Model::createVertexBuffer() {
        vk::BufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.size = VERTEX_BUFFER_SIZE;
        bufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer;

        vma::AllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = vma::MemoryUsage::eAutoPreferDevice;

        std::tie(vertexBuffer, vertexAlloc) = allocator.createBuffer(bufferCreateInfo, allocCreateInfo);
    }

} // namespace blob
