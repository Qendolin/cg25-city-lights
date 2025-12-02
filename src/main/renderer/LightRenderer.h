#pragma once
#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../debug/Annotation.h"
#include "../debug/Settings.h"


struct BufferBase;
namespace scene {
    struct GpuData;
}
struct ImageViewPairBase;
class ShaderLoader;

class LightRenderer {

public:
    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr CombinedImageSamplerBinding InDepth{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding TileLightIndices{1, vk::ShaderStageFlagBits::eCompute};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, InDepth, TileLightIndices);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "light_renderer_descriptor_layout");
        }
    };

    struct alignas(16) ShaderPushConstants {
        glm::mat4 inverseViewMatrix;
        glm::vec2 inverseProjectionScale;
        glm::vec2 inverseProjectionOffset;
        float zNear;
        float lightRangeFactor;
        float pad1;
        float pad2;
    };

    float lightRangeFactor = 1.0f;

    ~LightRenderer();
    explicit LightRenderer(const vk::Device &device);


    void recreate(const vk::Device &device, const ShaderLoader &shader_loader) {
        createPipeline(device, shader_loader);
    }

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const scene::GpuData &gpu_data,
            const glm::mat4 &projection_mat,
            const glm::mat4 &view_mat,
            float z_near,
            const ImageViewPairBase &depth_attachment,
            const BufferBase &tile_light_indices_buffer
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);

    static void calculateInverseProjectionConstants(
            const glm::mat4 &projectionMatrix, float textureWidth, float textureHeight, glm::vec2 &viewScale, glm::vec2 &viewOffset
    );

    vk::UniqueSampler mDepthSampler;
    ConfiguredComputePipeline mPipeline;
    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
};
