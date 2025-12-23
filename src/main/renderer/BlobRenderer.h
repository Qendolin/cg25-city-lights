#pragma once

#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Framebuffer.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../blob/Model.h"
#include "../debug/Annotation.h"
#include "../entity/Camera.h"

class BlobRenderer {
public:
    struct ComputeDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding VERTICES_BINDING{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding INDIRECT_DRAW_BINDING{1, vk::ShaderStageFlagBits::eCompute};

        ComputeDescriptorLayout() = default;

        explicit ComputeDescriptorLayout(const vk::Device &device) {
            create(device, {}, VERTICES_BINDING, INDIRECT_DRAW_BINDING);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "blob_renderer_compute_descriptor_layout");
        }
    };

    struct DrawDescriptorLayout : DescriptorSetLayout {
        static constexpr CombinedImageSamplerBinding COLOR_IMAGE_BINDING{0, vk::ShaderStageFlagBits::eFragment};

        DrawDescriptorLayout() = default;

        explicit DrawDescriptorLayout(const vk::Device &device) {
            create(device, {}, COLOR_IMAGE_BINDING);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "blob_renderer_draw_descriptor_layout");
        }
    };

    struct ComputePushConstant {
        int resolution;
        float time;
        float groundLevel;
        float size;
    };

    struct VertexFragmentPushConstant {
        glm::mat4 projectionMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 modelMatrix;
        glm::vec4 camera;
        glm::vec2 invViewportSize;
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
            const blob::Model &blobModel,
            float timestamp
    );

private:
    void createComputePipeline_(const vk::Device &device, const ShaderLoader &shaderLoader);
    void createGraphicsPipeline_(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer);

    void computeVertices(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &commandBuffer,
            const blob::Model &blobModel,
            float timestamp
    );

    void renderVertices(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &commandBuffer,
            const Framebuffer &framebuffer,
            const ImageViewPairBase &storedColorImage,
            const Camera &camera,
            const blob::Model &blobModel
    );
};
