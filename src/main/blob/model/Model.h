#pragma once

#include "VertexData.h"

#include <vulkan/vulkan.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

namespace blob {

    class Model {
    private:
        static constexpr int MAX_VERTICES = 10000; // TODO Temp
        static constexpr vk::DeviceSize VERTEX_BUFFER_SIZE = sizeof(VertexData) * MAX_VERTICES;

        const vma::Allocator &allocator;

        uint32_t vertexCount;

        void *stagingData = nullptr;
        vk::Buffer vertexStagingBuffer;
        vma::Allocation vertexStagingAlloc;

        vk::Buffer vertexBuffer;
        vma::Allocation vertexAlloc;

    public:
        Model(const vma::Allocator &device);
        ~Model();

        Model(const Model &) = delete;
        Model &operator=(const Model &) = delete;

        void updateVertices(vk::CommandBuffer commandBuffer, const std::vector<VertexData> &vertices);
        void bind(vk::CommandBuffer commandBuffer) const;
        void draw(vk::CommandBuffer commandBuffer) const;

    private:
        void createVertexStagingBuffer();
        void createVertexBuffer();
    };

} // namespace blob
