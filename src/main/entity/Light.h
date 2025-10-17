#pragma once

#include <glm/glm.hpp>

struct DirectionalLight {
    // degrees
    float elevation = 90;
    // degrees
    float azimuth = 0;
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float power = 1.0f;

    [[nodiscard]] glm::vec3 direction() const {
        float az_rad = glm::radians(azimuth);
        float el_rad = glm::radians(elevation);
        return glm::vec3{
            glm::sin(az_rad) * glm::cos(el_rad),
            glm::sin(el_rad),
            glm::cos(az_rad) * glm::cos(el_rad),
        };
    }

    [[nodiscard]] glm::vec3 radiance() const {
        return color * power;
    }
};