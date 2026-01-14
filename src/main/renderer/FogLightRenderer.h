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

class FogLightRenderer {

public:
    static constexpr uint32_t CLUSTER_DIM_X = 32;
    static constexpr uint32_t CLUSTER_DIM_Y = 18; // Approx 16:9 ratio
    static constexpr uint32_t CLUSTER_DIM_Z = 24; // Depth slices
    static constexpr uint32_t CLUSTER_LIGHT_STRIDE = 128;

    static constexpr uint32_t CLUSTER_BUFFER_SIZE = CLUSTER_DIM_X * CLUSTER_DIM_Y * CLUSTER_DIM_Z *
                                                    CLUSTER_LIGHT_STRIDE * sizeof(uint32_t);

    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding TileLightIndices{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding UberLights{1, vk::ShaderStageFlagBits::eCompute};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, TileLightIndices, UberLights);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "fog_light_renderer_descriptor_layout");
        }
    };

    struct alignas(16) ShaderPushConstants {
        glm::mat4 inverseViewMatrix;
        glm::vec2 inverseProjectionScale;
        glm::vec2 inverseProjectionOffset;
        glm::vec3 cameraPosition;
        float zNear;
        glm::vec3 cameraForward;
        float pad0 = 0;
    };

    ~FogLightRenderer();
    explicit FogLightRenderer(const vk::Device &device);


    void recreate(const vk::Device &device, const ShaderLoader &shader_loader) {
        createPipeline(device, shader_loader);
    }

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const vk::Buffer &light_buffer,
            const glm::mat4 &projection_mat,
            const glm::mat4 &view_mat,
            float z_near,
            const BufferBase &cluster_light_indices_buffer
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);

    static void calculateInverseProjectionConstants(
            const glm::mat4 &projectionMatrix, float textureWidth, float textureHeight, glm::vec2 &viewScale, glm::vec2 &viewOffset
    );

    ConfiguredComputePipeline mPipeline;
    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
};
