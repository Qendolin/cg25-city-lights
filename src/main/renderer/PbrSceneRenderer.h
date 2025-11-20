#pragma once
#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../debug/Settings.h"
#include "../util/PerFrame.h"


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
        static constexpr CombinedImageSamplerBinding SunShadowMap{1, vk::ShaderStageFlagBits::eFragment, Settings::SHADOW_CASCADE_COUNT};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, SceneUniforms, SunShadowMap);
        }
    };

    ~PbrSceneRenderer();
    PbrSceneRenderer(const vk::Device &device, const DescriptorAllocator &allocator);

    void recreate(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb) {
        createPipeline(device, shader_loader, fb);
    }

    void execute(
            const vk::Device &device,
            const vk::CommandBuffer &cmd_buf,
            const Framebuffer &fb,
            const Camera &camera,
            const scene::GpuData &gpu_data,
            const DirectionalLight &sun_light,
            std::span<ShadowCaster> sun_shadow_cascades,
            const glm::vec3& ambient_light
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb);

    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
    util::PerFrame<DescriptorSet> mShaderParamsDescriptors;

    ConfiguredGraphicsPipeline mPipeline;
    vk::UniqueSampler mShadowSampler;
};
