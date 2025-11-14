#pragma once

#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Framebuffer.h"
#include "../backend/Image.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../blob/Model.h"
#include "../entity/Camera.h"
#include "../util/PerFrame.h"

class BlobRenderer {
public:
    struct ComputeDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding VERTICES_BINDING{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding INDIRECT_DRAW_BINDING{1, vk::ShaderStageFlagBits::eCompute};

        ComputeDescriptorLayout() = default;

        explicit ComputeDescriptorLayout(const vk::Device &device) {
            create(device, {}, VERTICES_BINDING, INDIRECT_DRAW_BINDING);
        }
    };

    struct ComputePushConstant {
        int resolution;
        float time;
    };

    struct VertexFragmentPushConstant {
        glm::mat4 projectionViewModel{1.f};
        glm::mat4 ModelMatrix{1.f};
    };

private:
    static constexpr uint32_t WORK_GROUP_SIZE = 4;

    ConfiguredComputePipeline mComputePipeline;
    ConfiguredGraphicsPipeline mGraphicsPipeline;

    ComputeDescriptorLayout mComputeDescriptorLayout;
    util::PerFrame<DescriptorSet> mComputeDescriptors;

public:
    BlobRenderer(const vk::Device &device, const DescriptorAllocator &allocator);
    ~BlobRenderer() = default;

    void recreate(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer);

    void execute(
            const vk::Device &device,
            const vk::CommandBuffer &commandBuffer,
            const Framebuffer &framebuffer,
            const Camera &camera,
            const blob::Model &blobModel
    );

private:
    void createComputePipeline_(const vk::Device &device, const ShaderLoader &shaderLoader);
    void createGraphicsPipeline_(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer);

    void computeVertices(const vk::Device &device, const vk::CommandBuffer &commandBuffer, const blob::Model &blobModel);
    void renderVertices(
            const vk::CommandBuffer &commandBuffer, const Framebuffer &framebuffer, const Camera &camera, const blob::Model &blobModel
    );
};
