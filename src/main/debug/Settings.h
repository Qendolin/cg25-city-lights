#pragma once
#include "../scene/Light.h"

struct Settings {
    DirectionalLight sun = {
        .elevation = 20.0f,
        .azimuth = 0.0f,
        .color = glm::vec3{1.0f, 1.0f, 1.0f},
        .power = 15.0f
    };
};