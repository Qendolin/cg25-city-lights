#pragma once

#include <GLM/glm.hpp>
#include <vector>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "VertexData.h"

namespace blob {

    class Model {
    public:
        float groundLevel{-1.f};
        float size{1.f};
    
    private:
        static constexpr int MAX_VERTICES_PER_CELL = 12;
        static constexpr float TWO_PI = 6.2831855f;
        static constexpr float MAX_ANIMATION_TIME = TWO_PI;

        const int mResolution;

        const vma::Allocator &mAllocator;

        vk::Buffer mVertexBuffer;
        vma::Allocation mVertexAlloc;

        vk::Buffer mIndirectDrawBuffer;
        vma::Allocation mIndirectDrawAlloc;

        glm::mat4 mTransform = 1.f;

    public:
        Model(const vma::Allocator &allocator, const vk::Device &device, int resolution, const glm::mat4 &transform);
        ~Model();

        Model(const Model &) = delete;
        Model &operator=(const Model &) = delete;

        int getResolution() const { return mResolution; }
        vk::Buffer getVertexBuffer() const { return mVertexBuffer; }
        vk::Buffer getIndirectDrawBuffer() const { return mIndirectDrawBuffer; }
        glm::mat4 getTransform() const { return mTransform; }
        void setTransform(glm::mat4 transform) { mTransform = transform; }

    private:
        void createVertexBuffer();
        void createIndirectDrawBuffer();
    };

} // namespace blob
