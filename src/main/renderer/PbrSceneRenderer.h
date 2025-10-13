#pragma once
#include <glm/glm.hpp>

#include "../backend/Descriptors.h"
#include "../backend/Pipeline.h"
#include "../scene/gpu_types.h"
#include "../util/PerFrame.h"


namespace scene {
    struct GpuData;
}
class Framebuffer;
class ShaderLoader;
class Swapchain;
class PbrSceneRenderer {
public:

    struct alignas(16) ShaderParamsInlineUniformBlock {
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec4 camera; // Padded to 16 bytes
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
            const ShaderLoader &shader_loader,
            const Swapchain &swapchain
    );

    void prepare(const vk::Device &device, const Framebuffer &fb, const scene::GpuData &gpu_data) const;

    void render(const vk::CommandBuffer &cmd_buf, const Framebuffer &fb, const scene::GpuData &gpu_data);

private:
    void createDescriptors(const vk::Device &device, const DescriptorAllocator &allocator, const Swapchain &swapchain);

    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Swapchain &swapchain);

    ShaderParamsDescriptorLayout mShaderParamsDescriptorLayout;
    scene::SceneDescriptorLayout mSceneDescriptorLayout;

    util::PerFrame<DescriptorSet> mSceneDescriptors;
    util::PerFrame<DescriptorSet> mShaderParamsDescriptors;

    ConfiguredPipeline mPipeline;
};
