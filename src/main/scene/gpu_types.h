#pragma once

#include <glm/mat4x4.hpp>

struct alignas(16) InstanceBlock {
    glm::mat4 transform;
};

struct alignas(4) SectionBlock {
    glm::uint instance;
    glm::uint material;
};

struct alignas(16) MaterialBlock {
    glm::vec4 albedoFactors;
    glm::vec4 mrnFactors; // Padded to 16 bytes
};

