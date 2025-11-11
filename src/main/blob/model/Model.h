#pragma once

#include <GLM/glm.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#include "VertexData.h"

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

        glm::mat4 modelMatrix{1.f};

    public:
        Model(const vma::Allocator &device);
        ~Model();

        Model(const Model &) = delete;
        Model &operator=(const Model &) = delete;

        void updateVertices(vk::CommandBuffer commandBuffer, const std::vector<VertexData> &vertices);

        vk::Buffer getVertexBuffer() const { return vertexBuffer; }
        uint32_t getVertexCount() const { return vertexCount; }
        glm::mat4 getModelMatrix() const { return modelMatrix; }

    private:
        void createVertexStagingBuffer();
        void createVertexBuffer();
    };

} // namespace blob
