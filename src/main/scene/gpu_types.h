#pragma once

#include <glm/mat4x4.hpp>

#include "../backend/Descriptors.h"
#include "../debug/Annotation.h"

// Storage buffers use std430 layout alignment rules:
// scalar = 4
// vec2 = 8
// vec3, vec4 = 16
// mat2 = 8
// mat3, mat4 = 16
// array stride = align(element)
// struct align = max(member aligns)

struct alignas(16) InstanceBlock {
    glm::mat4 transform;
};

struct alignas(4) SectionBlock {
    glm::uint instance;
    glm::uint material;
};

struct alignas(16) BoundingBoxBlock {
    glm::vec4 min;
    glm::vec4 max;
};

struct alignas(16) MaterialBlock {
    glm::vec4 albedoFactors;
    // roughness, metalness, normal strength factors
    glm::vec4 rmnFactors; // Padded to 16 bytes
    // albedo, normal
    glm::uint packedImageIndices0;
    // orm, unused
    glm::uint packedImageIndices1;
    glm::uint pad0;
    glm::uint pad1;
};

struct alignas(16) UberLightBlock {
    glm::vec3 position = {};
    float range = 0.0f;
    glm::vec3 radiance = {};
    float coneAngleScale = 0.0f;
    glm::vec2 direction = {};
    float pointSize = 0.0f;
    float coneAngleOffset = 1.0f;

    static float calculateLightRange(glm::vec3 radiance, float pointSize, float epsilon = 0.001f) {
        float intensity = std::max({radiance.x, radiance.y, radiance.z});

        float term = (intensity / epsilon) - pointSize;
        if (term <= 0.0f) {
            return 0.0f;
        }
        return std::sqrt(term);
    }


    void updateRange(float epsilon = 0.001f) {
        range = calculateLightRange(radiance, pointSize, epsilon);
    }
};

namespace scene {
    struct SceneDescriptorLayout : DescriptorSetLayout {
        static constexpr StorageBufferBinding SectionBuffer{
            0, vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute
        };
        static constexpr StorageBufferBinding InstanceBuffer{
            1, vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute
        };
        static constexpr StorageBufferBinding MaterialBuffer{2, vk::ShaderStageFlagBits::eAllGraphics};
        static constexpr CombinedImageSamplerBinding ImageSamplers{
            3, vk::ShaderStageFlagBits::eAllGraphics, 65536, vk::DescriptorBindingFlagBits::ePartiallyBound
        };
        static constexpr StorageBufferBinding UberLightBuffer{4, vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute};
        static constexpr StorageBufferBinding BoundingBoxBuffer{
            6, vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute
        };

        SceneDescriptorLayout() = default;

        explicit SceneDescriptorLayout(const vk::Device &device) {
            create(device, {}, SectionBuffer, InstanceBuffer, MaterialBuffer, ImageSamplers, UberLightBuffer, BoundingBoxBuffer);
            util::setDebugName(device, vk::DescriptorSetLayout(*this), "scene_descriptor_layout");
        }
    };
} // namespace scene
