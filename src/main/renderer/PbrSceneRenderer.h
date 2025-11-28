#pragma once
#include <glm/glm.hpp>

#include "../backend/Buffer.h"
#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../debug/Settings.h"
#include "FrustumCuller.h"


struct ImageViewPairBase;
class CascadedShadowCaster;
class FrustumCuller;
class ShadowCaster;
struct DirectionalLight;
class Camera;
namespace scene {
    struct GpuData;
}
class Framebuffer;
class ShaderLoader;

class PbrSceneRenderer {
public:
    struct alignas(16) ShaderParamsInlineUniformBlock {
        struct alignas(16) DirectionalLight {
            glm::vec4 radiance;
            glm::vec4 direction;
        };
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec4 camera; // Padded to 16 bytes
        glm::vec4 viewport;
        DirectionalLight sun;
        glm::vec4 ambient;
    };

    struct alignas(4) ShaderPushConstants {
        union FlagsUnion {
            uint32_t value;

            struct FlagBits {
                unsigned int shadowCascades: 1;
                unsigned int whiteWorld: 1;
                unsigned int unused: 30;
            } bits;
        } flags;
    };


    struct alignas(16) ShadowCascadeUniformBlock {
        glm::mat4 projectionView;
        float sampleBias;
        float sampleBiasClamp;
        float normalBias;
        float distance;
    };

    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr InlineUniformBlockBinding SceneUniforms{
            0, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, sizeof(ShaderParamsInlineUniformBlock)
        };
        static constexpr CombinedImageSamplerBinding SunShadowMap{
            1, vk::ShaderStageFlagBits::eFragment, Settings::SHADOW_CASCADE_COUNT
        };
        static constexpr UniformBufferBinding ShadowCascadeUniforms{
            2,  vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
        };
        static constexpr CombinedImageSamplerBinding AmbientOcclusion{
            3, vk::ShaderStageFlagBits::eFragment
        };

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, SceneUniforms, SunShadowMap, ShadowCascadeUniforms, AmbientOcclusion);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "pbr_scene_renderer_descriptor_layout");
        }
    };

    bool pauseCulling = false;
    bool enableCulling = true;

    ~PbrSceneRenderer();
    explicit PbrSceneRenderer(const vk::Device &device);

    void recreate(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb);

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &desc_alloc,
            const TransientBufferAllocator &buf_alloc,
            const vk::CommandBuffer &cmd_buf,
            const Framebuffer &fb,
            const Camera &camera,
            const scene::GpuData &gpu_data,
            const FrustumCuller &frustum_culler,
            const DirectionalLight &sun_light,
            std::span<const CascadedShadowCaster> sun_shadow_cascades,
            const ImageViewPairBase& ao_result,
            const Settings& settings
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb);

    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;

    ConfiguredGraphicsPipeline mPipeline;
    vk::UniqueSampler mShadowSampler;
    vk::UniqueSampler mAoSampler;

    std::optional<glm::mat4> mCapturedFrustum;
};
