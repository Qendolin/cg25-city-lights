#pragma once
#include <glm/glm.hpp>
#include <optional>

#include "../backend/Buffer.h"
#include "../backend/Pipeline.h"
#include "FrustumCuller.h"

class Camera;
class Framebuffer;
class ShaderLoader;
namespace scene {
    struct GpuData;
}

class DepthPrePassRenderer {
public:
    struct alignas(16) ShaderPushConstants {
        glm::mat4 view;
        glm::mat4 projection;
    };

    bool pauseCulling = false;
    bool enableCulling = true;

    ~DepthPrePassRenderer();
    DepthPrePassRenderer();

    void recreate(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb);

    void execute(
            const vk::Device &device,
            const DescriptorAllocator &desc_alloc,
            const TransientBufferAllocator &buf_alloc,
            const vk::CommandBuffer &cmd_buf,
            const Framebuffer &fb,
            const Camera &camera,
            const scene::GpuData &gpu_data,
            const FrustumCuller &frustum_culler
    );

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb);

    ConfiguredGraphicsPipeline mPipeline;
    std::optional<glm::mat4> mCapturedFrustum;
};