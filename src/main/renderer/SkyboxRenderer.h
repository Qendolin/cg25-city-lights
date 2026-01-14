#pragma once

#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Framebuffer.h"
#include "../backend/Pipeline.h"
#include "../debug/Annotation.h"
#include "../entity/Camera.h"
#include "../entity/Cubemap.h"
#include "../util/PerFrame.h"

class ShaderLoader;
namespace vk {
    class Device;
}

class SkyboxRenderer {
public:
    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr CombinedImageSamplerBinding SkyboxDay{0, vk::ShaderStageFlagBits::eFragment};
        static constexpr CombinedImageSamplerBinding SkyboxNight{1, vk::ShaderStageFlagBits::eFragment};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, SkyboxDay, SkyboxNight);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "skybox_renderer_descriptor_layout");
        }
    };

    struct ShaderParamsPushConstants {
        glm::mat4 projViewNoTranslation = {1.f};
        glm::vec4 tint = glm::vec4(1.0f);
        float blend;
        float pad0 = 0;
        float pad1 = 0;
        float pad2 = 0;
    };

private:
    static constexpr int SKYBOX_VERTEX_COUNT = 36;

    vk::UniqueSampler mSampler;
    ConfiguredGraphicsPipeline mPipeline;
    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;

public:
    SkyboxRenderer(const vk::Device &device);
    ~SkyboxRenderer();

    void recreate(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &framebuffer) {
        createPipeline(device, shaderLoader, framebuffer);
    }

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const vk::CommandBuffer &cmd_buf,
            const Framebuffer &framebuffer,
            const Camera &camera,
            const Cubemap &skyboxDay,
            const Cubemap &skyboxNight,
            float exposure,
            float dayNightBlend,
            const glm::vec3 &tint,
            float rotation
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &fb);
};
