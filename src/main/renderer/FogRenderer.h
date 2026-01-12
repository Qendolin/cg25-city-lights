#pragma once
#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../debug/Annotation.h"
#include "../debug/Settings.h"


class TransientBufferAllocator;
class CascadedShadowCaster;
struct ImageViewPairBase;
class ShaderLoader;
namespace vk {
    class Device;
}

class FogRenderer {

public:
    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr CombinedImageSamplerBinding InDepth{0, vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageImageBinding InOutColor{1, vk::ShaderStageFlagBits::eCompute};
        static constexpr CombinedImageSamplerBinding SunShadowMap{
            2, vk::ShaderStageFlagBits::eCompute, Settings::SHADOW_CASCADE_COUNT
        };
        static constexpr UniformBufferBinding ShadowCascadeUniforms{3, vk::ShaderStageFlagBits::eCompute};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, InDepth, InOutColor, SunShadowMap, ShadowCascadeUniforms);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "fog_renderer_descriptor_layout");
        }
    };

    struct PushConstants {
        glm::vec2 inverseProjectionScale;
        glm::vec2 inverseProjectionOffset;
        glm::vec3 sunUpVS;
        float zNear;
        glm::vec3 sunRightVS;
        float density;
        glm::vec3 sunRadiance;
        float stepSize;
        glm::vec3 ambientRadiance;
        float cameraHeight;
        glm::vec3 worldUpVS;
        float heightFalloff;
        glm::vec3 fogColor;
        glm::uint samples;
        glm::vec3 sunDirVS;
        float g;
        glm::int32_t depthSampleIndex;
        glm::uint frame;
        float pad0 = 0;
        float pad1 = 0;
    };

    struct alignas(16) ShadowCascadeUniformBlock {
        glm::mat4 transform;
        glm::vec2 boundsMin;
        glm::vec2 boundsMax;
    };

    uint32_t samples = 32;
    float stepSize = 0.2f;
    float density = 0.001f;
    float heightFalloff = 0.1f;
    float g = 0.7f;

    ~FogRenderer();
    explicit FogRenderer(const vk::Device &device);

    void recreate(const vk::Device &device, const ShaderLoader &shader_loader) {
        createPipeline(device, shader_loader);
    }

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &descriptor_allocator,
            const TransientBufferAllocator &buffer_allocator,
            const vk::CommandBuffer &cmd_buf,
            const ImageViewPairBase &depth_attachment,
            const ImageViewPairBase &hdr_result_image,
            const DirectionalLight &sun_light,
            const glm::vec3& ambient_light,
            const glm::vec3& fog_color,
            std::span<const CascadedShadowCaster> sun_shadow_cascades,
            const glm::mat4 &view_mat,
            const glm::mat4 &projection_mat,
            float z_near,
            uint32_t frame_nr
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);

    static void calculateInverseProjectionConstants(
            const glm::mat4 &projectionMatrix, float textureWidth, float textureHeight, glm::vec2 &viewScale, glm::vec2 &viewOffset
    );


    vk::UniqueSampler mDepthSampler;
    vk::UniqueSampler mShadowSampler;
    ConfiguredComputePipeline mPipeline;
    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
};
