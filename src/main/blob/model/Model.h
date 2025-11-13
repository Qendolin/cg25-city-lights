#pragma once

#include <GLM/glm.hpp>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>
#include <vector>

#include "VertexData.h"

namespace blob {

    class Model {
    private:
        static constexpr std::size_t MAX_VERTICES = 9999;
        static constexpr vk::DeviceSize VERTEX_BUFFER_SIZE = sizeof(VertexData) * MAX_VERTICES;

        std::vector<VertexData> vertices;

        const vma::Allocator &allocator;

        void *stagingData = nullptr;
        vk::Buffer vertexStagingBuffer;
        vma::Allocation vertexStagingAlloc;

        vk::Buffer vertexBuffer;
        vma::Allocation vertexAlloc;

        glm::mat4 modelMatrix{1.f};

    public:
        Model(const vma::Allocator &allocator);
        ~Model();

        Model(const Model &) = delete;
        Model &operator=(const Model &) = delete;
        
        void setVertices(std::vector<VertexData> &&newVertices) noexcept { vertices = std::move(newVertices); }

        vk::Buffer getVertexBuffer() const { return vertexBuffer; }
        uint32_t getVertexCount() const { return vertices.size(); }
        glm::mat4 getModelMatrix() const { return modelMatrix; }

        void pushVertices(vk::CommandBuffer commandBuffer) const;

    private:
        void createVertexStagingBuffer();
        void createVertexBuffer();
    };

} // namespace blob
