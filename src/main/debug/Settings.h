#pragma once
#include <array>

#include "../entity/Light.h"

class ShadowCaster;

struct Settings {
    static constexpr int SHADOW_CASCADE_COUNT = 5;
    DirectionalLight sun = {.elevation = 40.0f, .azimuth = 10.0f, .color = glm::vec3{1.0f, 1.0f, 1.0f}, .power = 15.0f};
    struct Sky {
        float exposure = 1.49f;
    } sky;
    struct Shadow {
        int resolution = 2048;
        float dimension = 20.0f;
        float start = -1000.0f;
        float end = 1000.0f;
        float extrusionBias = -0.5f;
        float normalBias = 7.0f;
        float sampleBias = 0.1f;
        float sampleBiasClamp = 0.02f;
        float depthBiasConstant = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlope = -2.5f;

        void applyTo(ShadowCaster &caster) const;
    };
    std::array<Shadow, SHADOW_CASCADE_COUNT> shadowCascades;
    struct AgXParams {
        float ev_min = -12.47393f;
        float ev_max = 4.026069f;
        float mid_gray = 1.0f;
        float offset = 0.02f;
        float slope = 0.98f;
        float power = 1.2f;
        float saturation = 1.0f;
    } agx;
    struct Rendering {
        glm::vec3 ambient = glm::vec3(5.0f);
    } rendering;

    Settings() {
        shadowCascades[0] = {
            .dimension = 12,
            .extrusionBias = 0.0f,
            .normalBias = 0.0f,
            .sampleBiasClamp = 0.05f,
            .depthBiasSlope = 0.0f,
        };
        shadowCascades[1].dimension = 24;
        shadowCascades[2].dimension = 48;
        shadowCascades[3].dimension = 128;
        shadowCascades[4].dimension = 256;
    }
};

