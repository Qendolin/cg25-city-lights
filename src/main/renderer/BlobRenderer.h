#pragma once

#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Framebuffer.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../debug/Annotation.h"
#include "../entity/Camera.h"

namespace blob {
    class System;
}
class BlobRenderer {
public:
    struct ComputeDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding MetaballBuffer{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding VertexBuffer{1, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding IndirectBuffer{2, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding DomainMemberBuffer{3, vk::ShaderStageFlagBits::eCompute};

        ComputeDescriptorLayout() = default;

        explicit ComputeDescriptorLayout(const vk::Device &device) {
            create(device, {}, MetaballBuffer, VertexBuffer, IndirectBuffer, DomainMemberBuffer);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "blob_renderer_compute_descriptor_layout");
        }
    };


    struct DrawInlineUniformBlock {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 camera;
        glm::vec2 invViewportSize;
    };

    struct DrawDescriptorLayout : DescriptorSetLayout {
        static constexpr CombinedImageSamplerBinding StoredColorImage{0, vk::ShaderStageFlagBits::eFragment};
        static constexpr InlineUniformBlockBinding ShaderParams{
            1, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex, sizeof(DrawInlineUniformBlock)
        };

        DrawDescriptorLayout() = default;

        explicit DrawDescriptorLayout(const vk::Device &device) {
            create(device, {}, StoredColorImage, ShaderParams);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "blob_renderer_draw_descriptor_layout");
        }
    };

    struct alignas(16) ComputePushConstant {
        glm::vec3 aabbMin;
        float cellSize;
        glm::vec3 aabbMax;
        float time;
        glm::vec3 globalGridOrigin;
        glm::uint metaballIndexOffset;
        glm::uint metaballCount;
        float groundLevel;
        glm::uint drawIndex;
        glm::uint firstVertex;
    };

private:
    static constexpr uint32_t WORK_GROUP_SIZE = 4;

    ConfiguredComputePipeline mComputePipeline;
    ConfiguredGraphicsPipeline mGraphicsPipeline;

    ComputeDescriptorLayout mComputeDescriptorLayout;
    DrawDescriptorLayout mDrawDescriptorLayout;

    vk::UniqueSampler mSampler;

public:
    explicit BlobRenderer(const vk::Device &device);
    ~BlobRenderer() = default;

    void recreate(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer);

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &commandBuffer,
            const Framebuffer &framebuffer,
            const ImageViewPairBase &storedColorImage,
            const Camera &camera,
            const blob::System &blobSystem,
            float timestamp
    );

private:
    void createComputePipeline_(const vk::Device &device, const ShaderLoader &shaderLoader);
    void createGraphicsPipeline_(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer);

    void computeVertices(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const blob::System &blobSystem,
            float timestamp
    );

    void renderVertices(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const Framebuffer &framebuffer,
            const ImageViewPairBase &storedColorImage,
            const Camera &camera,
            const blob::System &blobSystem
    );
};
