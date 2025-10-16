#pragma once
#include <glm/glm.hpp>

#include "../backend/Framebuffer.h"
#include "../backend/Image.h"
#include "../backend/Pipeline.h"

namespace scene {
    struct GpuData;
}
class Framebuffer;
struct DirectionalLight;
class ShaderLoader;

class ShadowCaster {
public:
    static constexpr vk::Format DepthFormat = vk::Format::eD32Sfloat;

    /// <summary>The size of the shadow area.</summary>
    float dimension;
    /// <summary>The shadow start distance, from the caster's position.</summary>
    float start;
    /// <summary>The shadow end distance, from the caster's position.</summary>
    float end;

    /// <summary>Expands or shrinks the objects by offsetting the vertices along their normals.</summary>
    float extrusionBias = 0.0f;
    /// <summary>Offsets the shadow sample position based on the vertex normal.</summary>
    float normalBias = 0.0f;
    /// <summary>The bias used for the shadow comparison.</summary>
    float sampleBias = 0.0f;
    float sampleBiasClamp = 0.01f;
    /// <summary>See Vulkan's depthBiasConstantFactor.</summary>
    float depthBiasConstant = 0.0f;
    /// <summary>See Vulkan's depthBiasClamp.</summary>
    float depthBiasClamp = 0.0f;
    /// <summary>See Vulkan's depthBiasSlopeFactor.</summary>
    float depthBiasSlope = 0.0f;

    ShadowCaster(
            const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution, float dimension, float start, float end
    );

    [[nodiscard]] glm::mat4 viewMatrix() const { return mViewMatrix; }

    [[nodiscard]] glm::mat4 projectionMatrix() const;

    [[nodiscard]] const Framebuffer &framebuffer() const { return mFramebuffer; }

    void lookAt(const glm::vec3 &target, const glm::vec3 &direction, float distance = 0, const glm::vec3 &up = {0, 1, 0});

    void lookAt(const glm::vec3 &target, float azimuth, float elevation, float distance = 0, const glm::vec3 &up = {0, 1, 0});

    [[nodiscard]] uint32_t resolution() const { return mResolution; }

private:

    uint32_t mResolution;
    Image mDepthImage;
    vk::UniqueImageView mDepthImageView;
    Framebuffer mFramebuffer;
    glm::mat4 mViewMatrix = glm::mat4(1.0f);
};

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

    void render(const vk::CommandBuffer &cmd_buf, const scene::GpuData &gpu_data, const ShadowCaster &shadow_caster);

private:
    void createPipeline(const vk::Device &device, const ShaderLoader &shader_loader);

    ConfiguredPipeline mPipeline;
};
