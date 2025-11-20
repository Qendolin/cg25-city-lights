#pragma once
#include <glm/glm.hpp>

#include "../backend/Framebuffer.h"
#include "../backend/Pipeline.h"


class ShadowCaster;
namespace scene {
    struct GpuData;
}
class Framebuffer;
struct DirectionalLight;
class ShaderLoader;

class ShadowRenderer {
public:
    struct alignas(16) ShaderParamsPushConstants {
        glm::mat4 projectionViewMatrix;
        float sizeBias;
        float pad0;
        float pad1;
        float pad2;
    };

    ~ShadowRenderer();
    ShadowRenderer();

    void recreate(const vk::Device &device, const ShaderLoader &shader_loader) {
        createPipeline(device, shader_loader);
    }

    void execute(const vk::CommandBuffer &cmd_buf, const scene::GpuData &gpu_data, const ShadowCaster &shadow_caster);

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);

    ConfiguredGraphicsPipeline mPipeline;
};
