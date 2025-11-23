#pragma once
#include <glm/glm.hpp>

#include "../backend/Buffer.h"
#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../debug/Settings.h"
#include "FrustumCuller.h"


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
        struct alignas(16) ShadowCascade {
            glm::mat4 projectionView;
            float sampleBias;
            float sampleBiasClamp;
            float normalBias;
            float dimension;
        };
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec4 camera; // Padded to 16 bytes
        DirectionalLight sun;
        std::array<ShadowCascade, Settings::SHADOW_CASCADE_COUNT> cascades;
        glm::vec4 ambient;
    };

    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr InlineUniformBlockBinding SceneUniforms{
            0, vk::ShaderStageFlagBits::eAllGraphics, sizeof(ShaderParamsInlineUniformBlock)
        };
        static constexpr CombinedImageSamplerBinding SunShadowMap{
            1, vk::ShaderStageFlagBits::eFragment, Settings::SHADOW_CASCADE_COUNT
        };

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, SceneUniforms, SunShadowMap);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "pbr_scene_renderer_descriptor_layout");
        }
    };

    bool pauseCulling = false;
    bool enableCulling = true;

    ~PbrSceneRenderer();
    PbrSceneRenderer(const vk::Device &device);

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
            std::span<ShadowCaster> sun_shadow_cascades,
            const glm::vec3 &ambient_light
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb);

    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;

    ConfiguredGraphicsPipeline mPipeline;
    vk::UniqueSampler mShadowSampler;

    std::optional<glm::mat4> mCapturedFrustum;
};
