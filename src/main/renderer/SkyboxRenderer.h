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
        static constexpr CombinedImageSamplerBinding SamplerCubeMap{0, vk::ShaderStageFlagBits::eFragment};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device &device) {
            create(device, {}, SamplerCubeMap);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "skybox_renderer_descriptor_layout");
        }
    };

    struct ShaderParamsPushConstants {
        glm::mat4 projViewNoTranslation = {1.f};
        glm::vec4 tint = glm::vec4(1.0f);
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
            const Cubemap &skybox,
            float exposure,
            const glm::vec3& tint
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shaderLoader, const Framebuffer &fb);
};
