#include "Model2.h"

namespace blob {

    Model2::Model2(const vma::Allocator &allocator, int resolution /*, float intervalStart, float intervalEnd, float isoValue*/)
        : allocator{allocator},
          resolution{resolution} {
          // intervalStart{intervalStart},
          // intervalEnd{intervalEnd},
          // isoValue{isoValue},
          // stepSize{(intervalEnd - intervalStart) / static_cast<float>(resolution)},
          // sdfSampleCount{static_cast<std::size_t>(resolution * resolution * resolution)} {
        // createSdfSampleBuffer();
        createVertexBuffer();
        // sdfSamples.resize(sdfSampleCount);
    }

    /*
    void Model2::sampleSdf(const Sdf &sdf) {
        float *dst = static_cast<float *>(sdfSampleMapping);

        for (int i = 0; i < resolution; ++i)
            for (int j = 0; j < resolution; ++j)
                for (int k = 0; k < resolution; ++k) {
                    glm::vec3 p = {intervalStart + i * stepSize, intervalStart + j * stepSize, intervalStart + k * stepSize};
                    std::size_t index = (static_cast<std::size_t>(k) * resolution + j) * resolution + i;
                    dst[index] = sdf.value(p);
                }
    }
    */

    Model2::~Model2() {
        vmaDestroyBuffer(allocator, vertexBuffer, vertexAlloc);
        // vmaDestroyBuffer(allocator, sdfSampleBuffer, sdfSampleAlloc);
    }

    /*
    void Model2::createSdfSampleBuffer() {
        vk::BufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.size = sdfSampleCount * sizeof(float);
        bufferCreateInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;

        vma::AllocationCreateInfo allocCreateInfo{};
        allocCreateInfo.flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                                vma::AllocationCreateFlagBits::eMapped;
        allocCreateInfo.usage = vma::MemoryUsage::eAuto;
        allocCreateInfo.requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible |
                                        vk::MemoryPropertyFlagBits::eHostCoherent;

        vma::AllocationInfo stagingInfo{};
        std::tie(sdfSampleBuffer, sdfSampleAlloc) = allocator.createBuffer(bufferCreateInfo, allocCreateInfo, stagingInfo);

        sdfSampleMapping = stagingInfo.pMappedData;
    }
    */

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
