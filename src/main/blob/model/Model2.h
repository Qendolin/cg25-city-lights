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

        const vma::Allocator &allocator;

        vk::Buffer vertexBuffer;
        vma::Allocation vertexAlloc;

        glm::mat4 modelMatrix{1.f};

    public:
        Model2(const vma::Allocator &allocator, int resolution);
        ~Model2();

        Model2(const Model2 &) = delete;
        Model2 &operator=(const Model2 &) = delete;

        int getResolution() const { return resolution; }
        vk::Buffer getVertexBuffer() const { return vertexBuffer; }
        glm::mat4 getModelMatrix() const { return modelMatrix; }
        
    private:
        void createVertexBuffer();
    };

} // namespace blob
