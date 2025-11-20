#pragma once

#include <GLM/glm.hpp>
#include <vector>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "VertexData.h"

namespace blob {

    class Model {
    private:
        static constexpr int MAX_VERTICES_PER_CELL = 12;
        static constexpr float TWO_PI = 6.2831855f;
        static constexpr float MAX_ANIMATION_TIME = TWO_PI;

        const int resolution;

        const vma::Allocator &allocator;

        vk::Buffer vertexBuffer;
        vma::Allocation vertexAlloc;

        vk::Buffer indirectDrawBuffer;
        vma::Allocation indirectDrawAlloc;

        glm::mat4 modelMatrix = 1.f;
        float time = 0.f;

    public:
        Model(const vma::Allocator &allocator, int resolution);
        ~Model();

        Model(const Model &) = delete;
        Model &operator=(const Model &) = delete;

        void advanceTime(float dt);

        int getResolution() const { return resolution; }
        vk::Buffer getVertexBuffer() const { return vertexBuffer; }
        vk::Buffer getIndirectDrawBuffer() const { return indirectDrawBuffer; }
        glm::mat4 getModelMatrix() const { return modelMatrix; }
        float getTime() const { return time; }
        
    private:
        void createVertexBuffer();
        void createIndirectDrawBuffer();
    };

} // namespace blob
