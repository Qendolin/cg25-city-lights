#pragma once

#include "animation/Timeline.h"
#include "debug/Settings.h"
#include "scene/Loader.h"
#include "util/Color.h"

static void resetUFOLights(scene::Scene& scene) {
    size_t offset = scene.cpu().lights.size() - scene::Loader::DYNAMIC_LIGHTS_RESERVATION;
    for (size_t i = 0; i < scene::Loader::DYNAMIC_LIGHTS_RESERVATION; i++) {
        auto& light = scene.cpu().lights[offset + i];

        light.position = glm::vec3{0, 0, 0};
        light.radiance = glm::vec3{0, 0, 0};
        light.pointSize = 0.0f;
        light.range = 0.0f;
    }
}

static void createUFOLights(scene::Scene& scene) {
    size_t offset = scene.cpu().lights.size() - scene::Loader::DYNAMIC_LIGHTS_RESERVATION;
    for (size_t i = 0; i < scene::Loader::DYNAMIC_LIGHTS_RESERVATION; i++) {
        float h = 128.0f;
        float s = 0.9f;
        float v = 0.9f;

        glm::vec3 center = {0, 0, 0};

        auto& light = scene.cpu().lights[offset + i];

        float theta = i * 0.1375f;
        float r = 2 + i * 0.07f;
        float y = -std::pow(i * 0.1f, 1.5f);
        light.position = center + glm::vec3{r * glm::sin(theta), y, r * glm::cos(theta)};
        light.radiance = color::hsv_to_rgb(glm::vec3{h,s,v}) * 4.0f;
        light.pointSize = 0.25f;
        light.updateRange(0.1f);
    }
}

struct Settings;
static void createSceneAnimation(Timeline &t, Settings& settings, scene::Scene& scene) {

    t.add_callback(0u, [&scene, &settings]() {
        Settings def{};
        settings.blob = def.blob;
        settings.fog = def.fog;
        settings.sun = def.sun;
        settings.sky = def.sky;
        settings.agx = def.agx;
        settings.blob.dispersionXZ = 0.2f;
        settings.blob.dispersionY = 0.7f;
        settings.blob.dispersionPower = 1.0f;
        settings.blob.animationSpeed = 2.0f;
        settings.blob.baseRadius = 0.1f;
        settings.blob.maxRadius = 0.4f;
        settings.animation.animateLights = true;
        resetUFOLights(scene);
    });

    t.add(707u, 0.0f).to(1.0f).during(1000).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(0.2f, 0.7f, f);
        settings.blob.maxRadius = glm::mix(0.4f, 0.6f, f);
        return false;
    });

    t.add(707u, 0.0f).to(1.0f).during(1000).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(0.2f, 0.7f, f);
        settings.blob.maxRadius = glm::mix(0.4f, 0.5f, f);
        return false;
    });

    t.add(732u, 0.0f).to(1.0f).during(300).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(0.7f, 0.9f, f);
        settings.blob.dispersionY = glm::mix(0.7f, 0.9f, f);
        settings.blob.dispersionPower = glm::mix(1.0f, 0.8f, f);
        return false;
    });

    t.add(920u, 0.0f).to(1.0f).during(300).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(0.9f, 1.5f, f);
        settings.blob.dispersionY = glm::mix(0.9f, 1.5f, f);
        settings.blob.dispersionPower = glm::mix(0.8f, 0.7f, f);
        settings.blob.maxRadius =  glm::mix(0.5f, 0.4f, f);
        settings.blob.animationSpeed = 4.0f;
        return false;
    });

    t.add(936u, 0.0f).to(1.0f).during(500).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(1.5f, 0.2f, f);
        settings.blob.dispersionY = glm::mix(1.5f, 0.5f, f);
        settings.blob.dispersionPower = glm::mix(0.7f, 1.0f, f);
        settings.blob.animationSpeed = 2.0f;
        return false;
    });

    // Night transition

    t.add(1100u, 0.0f).to(1.0f).during(7900).onStep([&settings](float f) {
        settings.sun.elevation = glm::mix(40.0f, -24.0f, f);
        settings.agx.ev_min = glm::mix(-12.47393f, -13.5f, f);
        settings.fog.heightFalloff = glm::mix(0.29f, 0.2f, f);
        settings.fog.density = glm::mix(0.06f, 0.2f, glm::smoothstep(20.0f, 0.0f, settings.sun.elevation));
        return false;
    });

    // TODO: Blob dispersion when exit agian.

    // Lights

    t.add_callback(2440u, [&scene, &settings]() {
        createUFOLights(scene);
        settings.animation.animateLights = true;
    });

    t.reset();

}
