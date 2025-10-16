#pragma once
#include "../scene/Light.h"

struct Settings {
    DirectionalLight sun = {
        .elevation = 20.0f,
        .azimuth = 0.0f,
        .color = glm::vec3{1.0f, 1.0f, 1.0f},
        .power = 15.0f
    };
    struct Shadow {
        float extents = 10.0f;
        float distance = 50.0f;
        float sizeBias = 2.0f;
        float normalBias = 100.0f;
        float sampleBias = 0.1f;
        float sampleBiasClamp = 0.01f;
        float depthBiasConstant = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlope = -2.5f;
    } shadow;
};