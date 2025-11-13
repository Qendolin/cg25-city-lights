#include "Model2.h"

namespace blob {

    Model2::Model2(const vma::Allocator &allocator, int resolution)
        : allocator{allocator},
          resolution{resolution} {
        createVertexBuffer();
    }

    Model2::~Model2() {
        vmaDestroyBuffer(allocator, vertexBuffer, vertexAlloc);
    }

    void Model2::createVertexBuffer() {
        vk::BufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.size = resolution * resolution * resolution * MAX_VERTICES_PER_CELL * sizeof(VertexData);
        bufferCreateInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
                                 vk::BufferUsageFlagBits::eVertexBuffer;

        vma::AllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.usage = vma::MemoryUsage::eAutoPreferDevice;

        std::tie(vertexBuffer, vertexAlloc) = allocator.createBuffer(bufferCreateInfo, allocCreateInfo);
    }

} // namespace blob
