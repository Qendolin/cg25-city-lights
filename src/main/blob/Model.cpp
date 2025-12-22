#include "Model.h"

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "../debug/Annotation.h"

namespace blob {

    Model::Model(const vma::Allocator &allocator, const vk::Device &device, int resolution, const glm::mat4 &transform)
        : mAllocator{allocator}, mResolution{resolution}, mTransform{transform} {
        createVertexBuffer();
        util::setDebugName(device, mVertexBuffer, "blob_vertex_buffer");
        createIndirectDrawBuffer();
        util::setDebugName(device, mIndirectDrawBuffer, "blob_indirect_draw_buffer");
    }

    Model::~Model() {
        vmaDestroyBuffer(mAllocator, mIndirectDrawBuffer, mIndirectDrawAlloc);
        vmaDestroyBuffer(mAllocator, mVertexBuffer, mVertexAlloc);
    }

    void Model::createVertexBuffer() {
        vk::BufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.size = mResolution * mResolution * mResolution * MAX_VERTICES_PER_CELL * sizeof(VertexData);
        bufferCreateInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
                                 vk::BufferUsageFlagBits::eVertexBuffer;

        vma::AllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = vma::MemoryUsage::eAutoPreferDevice;

        std::tie(mVertexBuffer, mVertexAlloc) = mAllocator.createBuffer(bufferCreateInfo, allocCreateInfo);
    }

    void Model::createIndirectDrawBuffer() {
        vk::BufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.size = sizeof(vk::DrawIndirectCommand);
        bufferCreateInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
                                 vk::BufferUsageFlagBits::eTransferDst;

        vma::AllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = vma::MemoryUsage::eAutoPreferDevice;

        std::tie(mIndirectDrawBuffer, mIndirectDrawAlloc) = mAllocator.createBuffer(bufferCreateInfo, allocCreateInfo);
    }
} // namespace blob
