#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

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

struct PointLight {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float power = 1.0f;

    [[nodiscard]] glm::vec3 radiance() const {
        // don't divide by 4pi
        return color * power;
    }
};

struct SpotLight {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    // degrees, elevation
    float theta = 90;
    // degrees, azimuth
    float phi = 0;
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float power = 1.0f;
    // degrees
    float outerConeAngle = 75.0f;
    // degrees
    float innerConeAngle = 0.0f;

    [[nodiscard]] glm::vec3 direction() const {
        float phi_rad = glm::radians(phi);
        float theta_rad = glm::radians(theta);
        return glm::vec3{
            glm::sin(phi_rad) * glm::cos(theta_rad),
            glm::sin(theta_rad),
            glm::cos(phi_rad) * glm::cos(theta_rad),
        };
    }

    [[nodiscard]] glm::vec3 radiance() const {
        // don't divide by 4pi
        return color * power;
    }
};