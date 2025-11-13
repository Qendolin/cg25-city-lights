#pragma once

#include <GLM/glm.hpp>
#include <vector>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "../sdf/Sdf.h"
#include "VertexData.h"

namespace blob {

    class Model2 {
    private:
        static constexpr int MAX_VERTICES_PER_CELL = 12;

        const int resolution;

        /*
        const float intervalStart;
        const float intervalEnd;
        const float isoValue;
        const float stepSize;
        */

        const vma::Allocator &allocator;

        /*
        const std::size_t sdfSampleCount;

        void *sdfSampleMapping = nullptr;
        vk::Buffer sdfSampleBuffer;
        vma::Allocation sdfSampleAlloc;
        */

        vk::Buffer vertexBuffer;
        vma::Allocation vertexAlloc;

        glm::mat4 modelMatrix{1.f};

    public:
        Model2(const vma::Allocator &allocator,
               int resolution /*, float intervalStart, float intervalEnd, float isoValue*/);
        ~Model2();

        Model2(const Model2 &) = delete;
        Model2 &operator=(const Model2 &) = delete;

        // void sampleSdf(const Sdf &sdf);

        int getResolution() const { return resolution; }
        vk::Buffer getVertexBuffer() const { return vertexBuffer; }
        // vk::Buffer getSdfSampleBuffer() const { return sdfSampleBuffer; }
        // uint32_t getVertexCount() const { return static_cast<uint32_t>(sdfSampleCount * MAX_VERTICES_PER_CELL); } // temp
        glm::mat4 getModelMatrix() const { return modelMatrix; }
        
    private:
        // void createSdfSampleBuffer();
        void createVertexBuffer();
    };

} // namespace blob
