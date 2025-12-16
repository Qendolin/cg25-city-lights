#pragma once

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "../backend/Framebuffer.h"
#include "../backend/Image.h"

namespace vma {
    class Allocator;
}

class ShadowCaster {
public:
    static constexpr vk::Format DepthFormat = vk::Format::eD16Unorm;

    glm::mat4 viewMatrix = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::mat4(1.0f);

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

    ShadowCaster() = default;
    ShadowCaster(const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution);
    virtual ~ShadowCaster() = default;

    ShadowCaster(ShadowCaster &&other) noexcept = default;
    ShadowCaster &operator=(ShadowCaster &&other) noexcept = default;

    [[nodiscard]] const Framebuffer &framebuffer() const { return mFramebuffer; }

    [[nodiscard]] uint32_t resolution() const { return mResolution; }

private:
    uint32_t mResolution = 0;
    Framebuffer mFramebuffer;
    Image mDepthImage;
    ImageView mDepthImageView;
};


class SimpleShadowCaster : public ShadowCaster {
public:
    SimpleShadowCaster() = default;
    SimpleShadowCaster(
            const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution, float radius, float start, float end
    );
    ~SimpleShadowCaster() override = default;

    SimpleShadowCaster(SimpleShadowCaster &&other) noexcept = default;
    SimpleShadowCaster &operator=(SimpleShadowCaster &&other) noexcept = default;

    void setExtentRadius(float radius);

    void setExtentDepth(float start, float end);

    void setExtentDepthStart(float start);

    void setExtentDepthEnd(float end);

    void setExtents(float radius, float start, float end);

    [[nodiscard]] float extentRadius() const { return mRadius; }
    [[nodiscard]] float extentDepthStart() const { return mStart; }
    [[nodiscard]] float extentDepthEnd() const { return mEnd; }

    void lookAt(const glm::vec3 &target, const glm::vec3 &direction, float distance = 0, const glm::vec3 &up = {0, 1, 0});

    void lookAt(const glm::vec3 &target, float azimuth, float elevation, float distance = 0, const glm::vec3 &up = {0, 1, 0});

private:
    float mRadius = 0;
    float mStart = 0;
    float mEnd = 0;

    void updateProjectionMatrix();
};

class CascadedShadowCaster : public ShadowCaster {
public:
    float distance = 0.0f;

    CascadedShadowCaster() = default;
    CascadedShadowCaster(const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution)
        : ShadowCaster(device, allocator, resolution) {}
    ~CascadedShadowCaster() override = default;

    CascadedShadowCaster(CascadedShadowCaster &&other) noexcept = default;
    CascadedShadowCaster &operator=(CascadedShadowCaster &&other) noexcept = default;
};

class ShadowCascade {
public:
    /// <summary>Controls cascade spacing.</summary>
    float lambda = 0.75f;

    /// <summary>The maximum shadow distance.</summary>
    float distance = 1000.0f;

    ShadowCascade(const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution, int count);

    void update(float frustum_fov, float frustum_aspect, glm::mat4 view_matrix, glm::vec3 light_dir);

    [[nodiscard]] std::span<const CascadedShadowCaster> cascades() const { return mCascades; }
    [[nodiscard]] std::span<CascadedShadowCaster> cascades() { return mCascades; }

private:
    std::vector<CascadedShadowCaster> mCascades;

    static float calculateSplitDistance(float lambda, float near_clip, float far_clip, float clip_range, float f);

    static glm::mat4 createTexelAlignedViewMatrix(glm::vec3 light_dir, uint32_t resolution, float radius, glm::vec3 frustum_center);
};
