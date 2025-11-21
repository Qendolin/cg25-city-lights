#pragma once

#include <glm/mat4x4.hpp>

#include "../backend/Descriptors.h"

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


struct alignas(16) PointLightBlock {
    glm::vec4 radiance;
    glm::vec4 position;
};

struct alignas(16) SpotLightBlock {
    glm::vec4 radiance;
    glm::vec4 position;
    glm::vec4 direction;
    float coneAngleScale;
    float coneAngleOffset;
    float pad0;
    float pad1;
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
        static constexpr StorageBufferBinding PointLightBuffer{4, vk::ShaderStageFlagBits::eAllGraphics};
        static constexpr StorageBufferBinding SpotLightBuffer{5, vk::ShaderStageFlagBits::eAllGraphics};
        static constexpr StorageBufferBinding BoundingBoxBuffer{
            6, vk::ShaderStageFlagBits::eAllGraphics | vk::ShaderStageFlagBits::eCompute
        };

        SceneDescriptorLayout() = default;

        explicit SceneDescriptorLayout(const vk::Device &device) {
            create(device, {}, SectionBuffer, InstanceBuffer, MaterialBuffer, ImageSamplers, PointLightBuffer,
                   SpotLightBuffer, BoundingBoxBuffer);
        }
    };
} // namespace scene
