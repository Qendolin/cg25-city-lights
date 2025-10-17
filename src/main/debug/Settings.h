#pragma once
#include "../scene/Light.h"

class ShadowCaster;

struct Settings {
    DirectionalLight sun = {.elevation = 20.0f, .azimuth = 0.0f, .color = glm::vec3{1.0f, 1.0f, 1.0f}, .power = 15.0f};
    struct Shadow {
        int resolution = 2048;
        float dimension = 20.0f;
        float start = -1000.0f;
        float end = 1000.0f;
        float extrusionBias = 5.0f;
        float normalBias = 7.0f;
        float sampleBias = 0.1f;
        float sampleBiasClamp = 0.02f;
        float depthBiasConstant = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlope = -2.5f;

        void applyTo(ShadowCaster &caster) const;
    } shadow;
    struct AgXParams {
        float ev_min = -12.47393f;
        float ev_max = 4.026069f;
        float mid_gray = 1.0f;
        float offset = 0.02f;
        float slope = 0.98f;
        float power = 1.2f;
        float saturation = 1.0f;
    } agx;
};

