#pragma once

#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Framebuffer.h"
#include "../backend/Image.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../blob/model/Model2.h"
#include "../entity/Camera.h"
#include "../util/PerFrame.h"

class BlobRenderer2 {
public:
    struct ComputeDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding VERTICES_BINDING{0, vk::ShaderStageFlagBits::eCompute};

        ComputeDescriptorLayout() = default;

        explicit ComputeDescriptorLayout(const vk::Device &device) {
            create(device, {}, VERTICES_BINDING);
        }
    };

    struct ComputePushConstant {
        int resolution;
    };

    struct VertexFragmentPushConstant {
        glm::mat4 projectionViewModel{1.f};
        glm::mat4 ModelMatrix{1.f};
    };

private:
    ConfiguredComputePipeline mComputePipeline;
    ConfiguredGraphicsPipeline mGraphicsPipeline;

    ComputeDescriptorLayout mComputeDescriptorLayout;
    util::PerFrame<DescriptorSet> mComputeDescriptors;

public:
    BlobRenderer2(const vk::Device &device, const DescriptorAllocator &allocator);
    ~BlobRenderer2() = default;

    void recreate(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer);

    void execute(
            const vk::Device &device,
            const vk::CommandBuffer &commandBuffer,
            const Framebuffer &framebuffer,
            const Camera &camera,
            const blob::Model2 &blobModel
    );

private:
    void createComputePipeline_(const vk::Device &device, const ShaderLoader &shaderLoader);
    void createGraphicsPipeline_(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer);

    void computeVertices(const vk::Device &device, const vk::CommandBuffer &commandBuffer, const blob::Model2 &blobModel);
    void renderVertices(
            const vk::CommandBuffer &commandBuffer, const Framebuffer &framebuffer, const Camera &camera, const blob::Model2 &blobModel
    );
};
