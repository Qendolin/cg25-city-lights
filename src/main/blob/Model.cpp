#include "Model.h"

#include <glm/gtc/matrix_transform.hpp>

namespace blob {

    Model::Model(const vma::Allocator &allocator, int resolution) : allocator{allocator}, resolution{resolution} {
        createVertexBuffer();
        createIndirectDrawBuffer();
        modelMatrix = glm::translate(modelMatrix, {0.f, 1.f, 1.5f});
    }

    Model::~Model() {
        vmaDestroyBuffer(allocator, indirectDrawBuffer, indirectDrawAlloc);
        vmaDestroyBuffer(allocator, vertexBuffer, vertexAlloc);
    }

    void Model::createVertexBuffer() {
        vk::BufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.size = resolution * resolution * resolution * MAX_VERTICES_PER_CELL * sizeof(VertexData);
        bufferCreateInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
                                 vk::BufferUsageFlagBits::eVertexBuffer;

        vma::AllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = vma::MemoryUsage::eAutoPreferDevice;

        std::tie(vertexBuffer, vertexAlloc) = allocator.createBuffer(bufferCreateInfo, allocCreateInfo);
    }

    void Model::createIndirectDrawBuffer() {
        vk::BufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.size = sizeof(vk::DrawIndirectCommand);
        bufferCreateInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
                                 vk::BufferUsageFlagBits::eTransferDst;

        vma::AllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = vma::MemoryUsage::eAutoPreferDevice;

        std::tie(indirectDrawBuffer, indirectDrawAlloc) = allocator.createBuffer(bufferCreateInfo, allocCreateInfo);
    }

    void Model::advanceTime(float dt) {
        time += dt;
        time -= std::floor(time);
    }

} // namespace blob
