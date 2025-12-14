#pragma once

#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Image.h"
#include "../backend/Pipeline.h"
#include "../debug/Annotation.h"

struct DeviceQueue;
class ShaderLoader;
class SSAORenderer {

public:
    struct alignas(16) ShaderParamsInlineUniformBlock {
        glm::mat4 projection;
        glm::vec2 inverseProjectionScale;
        glm::vec2 inverseProjectionOffset;
        float zNear;
        float radius;
        float bias;
        float pad0;
    };

    struct SamplerShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr InlineUniformBlockBinding ShaderParams{
            0, vk::ShaderStageFlagBits::eCompute, sizeof(ShaderParamsInlineUniformBlock)
        };
        static constexpr CombinedImageSamplerBinding InDepth{1, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageImageBinding OutRawAO{2, vk::ShaderStageFlagBits::eCompute};
        static constexpr SampledImageBinding InNoise{3, vk::ShaderStageFlagBits::eCompute};

        SamplerShaderParamsDescriptorLayout() = default;

        explicit SamplerShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, ShaderParams, InDepth, OutRawAO, InNoise);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "ssao_renderer_sampler_descriptor_layout");
        }
    };

    struct FilterShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr CombinedImageSamplerBinding InRawAO{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr CombinedImageSamplerBinding InDepth{1, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageImageBinding OutFilteredAO{2, vk::ShaderStageFlagBits::eCompute};

        FilterShaderParamsDescriptorLayout() = default;

        explicit FilterShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, InRawAO, InDepth, OutFilteredAO);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "ssao_renderer_filter_descriptor_layout");
        }
    };

    struct alignas(4) FilterShaderPushConstants {
        glm::vec2 direction;
        float zNear;
        float sharpness;
        float exponent;
    };

    float radius = 0.5f;
    float exponent = 2.0f;
    float bias = 0.01f;
    float filterSharpness = 50.0;

    ~SSAORenderer();
    SSAORenderer(const vk::Device &device, const vma::Allocator &allocator, const DeviceQueue &graphicsQueue);

    void filterPass(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const ImageViewPairBase &depth_attachment,
            const ImageViewPairBase &ao_input,
            const ImageViewPairBase &ao_output,
            const FilterShaderPushConstants &filter_params
    );

    void recreate(const vk::Device &device, const ShaderLoader &shader_loader, int slices, int samples, bool bentNormals) {
        createPipeline(device, shader_loader, slices, samples, bentNormals);
    }

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const glm::mat4 &projection_mat,
            float z_near,
            const ImageViewPairBase &depth_attachment,
            const ImageViewPairBase &ao_intermediary,
            const ImageViewPairBase &ao_result
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, int slices, int samples, bool bentNormals);

    static void calculateInverseProjectionConstants(
            const glm::mat4 &projectionMatrix, float textureWidth, float textureHeight, glm::vec2 &viewScale, glm::vec2 &viewOffset
    );

    vk::UniqueSampler mDepthSampler;
    vk::UniqueSampler mAoSampler;
    ImageWithView mNoise;

    ConfiguredComputePipeline mSamplerPipeline;
    SamplerShaderParamsDescriptorLayout mSamplerShaderParamsDescriptorLayout;

    ConfiguredComputePipeline mFilterPipeline;
    FilterShaderParamsDescriptorLayout mFilterShaderParamsDescriptorLayout;


};
