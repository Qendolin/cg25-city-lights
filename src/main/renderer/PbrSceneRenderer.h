#pragma once
#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../util/PerFrame.h"


struct DirectionalLight;
class Camera;
namespace scene {
    struct GpuData;
}
class Framebuffer;
class ShaderLoader;
class Swapchain;

class PbrSceneRenderer {
public:

    struct alignas(16) ShaderParamsInlineUniformBlock {
        struct alignas(16) SunLight {
            glm::vec4 radiance; // Padded to 16 bytes
            glm::vec4 direction; // Padded to 16 bytes
        };
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec4 camera; // Padded to 16 bytes
        SunLight sun;
    };

    struct ShaderParamsDescriptorLayout : DescriptorSetLayout {
        static constexpr InlineUniformBlockBinding SceneUniforms{0, vk::ShaderStageFlagBits::eAllGraphics, sizeof(ShaderParamsInlineUniformBlock)};

        ShaderParamsDescriptorLayout() = default;

        explicit ShaderParamsDescriptorLayout(const vk::Device& device) {
            create(device, {}, SceneUniforms);
        }
    };

    ~PbrSceneRenderer();
    PbrSceneRenderer(
            const vk::Device &device,
            const DescriptorAllocator &allocator,
            const Swapchain &swapchain
    );

    void recreate(const vk::Device &device, const ShaderLoader &shader_loader, const Swapchain &swapchain) {
        createPipeline(device, shader_loader, swapchain);
    }

    void prepare(const vk::Device &device, const Camera &camera, const DirectionalLight& sun_light) const;

    void render(const vk::CommandBuffer &cmd_buf, const Framebuffer &fb, const scene::GpuData &gpu_data);

private:
    void createDescriptors(const vk::Device &device, const DescriptorAllocator &allocator, const Swapchain &swapchain);

    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Swapchain &swapchain);

    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
    util::PerFrame<DescriptorSet> mShaderParamsDescriptors;

    ConfiguredPipeline mPipeline;
};
