#pragma once

#include "animation/Timeline.h"
#include "audio/AudioSystem.h"
#include "debug/Settings.h"
#include "scene/Loader.h"
#include "util/Color.h"


struct SceneAudio {
    std::unique_ptr<Music> ambientMusic;
    std::unique_ptr<Sound> engineSoundBus;
    std::unique_ptr<Sound> engineSoundAlt;
    std::unique_ptr<Sound> ufoSound;
    std::unique_ptr<Sound> lidShutSound;
    std::unique_ptr<Sound> dumpsterOpenSound;
    std::unique_ptr<Sound> beamSound;
    std::unique_ptr<SoundInstance3d> engineSoundInstanceBlueCar;
    std::unique_ptr<SoundInstance3d> engineSoundInstanceBlueVan;
    std::unique_ptr<SoundInstance3d> engineSoundInstanceWhiteVan;
    std::unique_ptr<SoundInstance3d> ufoSoundInstance;
    std::unique_ptr<SoundInstance3d> beamSoundInstance;
};

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
        light.radiance = color::hsv_to_rgb(glm::vec3{h,s,v}) * 2.0f;
        light.pointSize = 0.25f;
        light.updateRange(0.05f);
    }
}

struct Settings;
static void createSceneAnimation(Timeline &t, Settings& settings, scene::Scene& scene, SceneAudio& audio) {

    t.add_callback(1u, [&scene, &settings, &audio]() {
        Settings def{};
        settings.blob = def.blob;
        settings.fog = def.fog;
        settings.sun = def.sun;
        settings.sky = def.sky;
        settings.agx = def.agx;
        settings.audio = def.audio;
        settings.blob.dispersionXZ = 0.2f;
        settings.blob.dispersionY = 0.7f;
        settings.blob.dispersionPower = 1.0f;
        settings.blob.animationSpeed = 2.0f;
        settings.blob.baseRadius = 0.1f;
        settings.blob.maxRadius = 0.4f;
        settings.animation.animateLights = true;
        resetUFOLights(scene);

        settings.audio.whiteVanVolume = 1.0f;
        settings.audio.blueCarVolume = 1.0f;
        settings.audio.blueVanVolume = 1.0f;

        audio.engineSoundInstanceWhiteVan->seek(16.0);
        audio.engineSoundInstanceWhiteVan->play();
        audio.engineSoundInstanceBlueVan->seek(11.0);
        audio.engineSoundInstanceBlueVan->play();
        audio.beamSoundInstance->setVolume(0);
        audio.ambientMusic->play();
    });

    // Car sound off
    t.add(500u, 1.0f).to(0.0f).during(1000).onStep([&settings](float f) {
        settings.audio.blueCarVolume = f;
        settings.audio.blueVanVolume = f;
        settings.audio.whiteVanVolume = f;
        return false;
    });

    // Blob dumpster sounds
    t.add_callback(620u, [&scene, &audio]() {
        auto [inst_idx, _] = scene.cpu().non_mesh_instance_animation_map.at("Dumpster.Sound");
        glm::vec3 pos = scene.cpu().instances[inst_idx].transform[3];
        audio.dumpsterOpenSound->play3dEvent(pos, 1.0);
    });

    // Blob exit dumpster
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

    // Blob loop-de-loop
    t.add(732u, 0.0f).to(1.0f).during(300).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(0.7f, 0.9f, f);
        settings.blob.dispersionY = glm::mix(0.7f, 0.9f, f);
        settings.blob.dispersionPower = glm::mix(1.0f, 0.8f, f);
        return false;
    });

    // White van approach
    t.add(1020u, 0.0f).to(1.0f).during(500).onStep([&settings](float f) {
        settings.audio.whiteVanVolume = f;
        return false;
    });

    // Blob scare
    t.add(1036u, 0.0f).to(1.0f).during(600).to(0.0f).during(1200).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(0.9f, 1.5f, f);
        settings.blob.dispersionY = glm::mix(0.9f, 1.5f, f);
        settings.blob.dispersionPower = glm::mix(0.8f, 0.7f, f);
        settings.blob.maxRadius =  glm::mix(0.5f, 0.4f, f);
        settings.blob.animationSpeed = glm::mix(2.0f, 4.0f, f);
        return false;
    });

    // Blob enter dumpster
    t.add(1104u, 0.0f).to(1.0f).during(500).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(0.9f, 0.2f, f);
        settings.blob.dispersionY = glm::mix(0.9f, 0.5f, f);
        settings.blob.dispersionPower = glm::mix(0.8f, 1.0f, f);
        settings.blob.maxRadius = glm::mix(0.5f, 0.3f, f);
        settings.blob.animationSpeed = 2.0f;
        return false;
    });

    t.add_callback(1127u, [&audio, &scene] {
        auto [inst_idx, _] = scene.cpu().non_mesh_instance_animation_map.at("Dumpster.Sound");
        glm::vec3 pos = scene.cpu().instances[inst_idx].transform[3];
        audio.lidShutSound->play3dEvent(pos, 1.0);
    });

    // White van stop
    t.add(1150u, 1.0f).to(0.0f).during(500).onStep([&settings](float f) {
       settings.audio.whiteVanVolume = f;
        return false;
    });

    // Night transition
    t.add(1210u, 0.0f).to(1.0f).during(9200).onStep([&settings](float f) {
        settings.sun.elevation = glm::mix(40.0f, -24.0f, f);
        float sunset = glm::smoothstep(0.0f, -20.0f, settings.sun.elevation);
        settings.agx.ev_min = glm::mix(-12.47393f, -14.0f, sunset);
        settings.agx.ev_max = glm::mix(4.026069f, 1.0f, sunset);
        settings.fog.heightFalloff = glm::mix(0.29f, 0.2f, f);
        settings.fog.density = glm::mix(0.015f, 0.2f, sunset);
        settings.audio.ambientVolume = glm::mix(0.2f, 0.0f, sunset);
        return false;
    });

    // Blob exit again
    t.add(1528u, 0.0f).to(1.0f).during(1000).onStep([&settings](float f) {
        settings.blob.dispersionXZ = glm::mix(0.2f, 1.0f, f);
        settings.blob.dispersionY = glm::mix(0.5f, 0.6f, f);
        settings.blob.maxRadius = glm::mix(0.3f, 0.5f, f);
        settings.blob.animationSpeed = 2.0f;
        return false;
    });

    // Blue car approach
    t.add(1880u, 0.0f).to(1.0f).during(1000).onStep([&settings](float f) {
        settings.audio.blueCarVolume = f;
        return false;
    });

    // Blue car approach
    t.add(2024u, 1.0f).to(0.0f).during(1000).onStep([&settings](float f) {
        settings.audio.blueCarVolume = f;
        return false;
    });

    // UFO appear
    t.add_callback(2200u, [&settings] {
       settings.audio.ufoVolume = 4.0;
    });

    // Lights
    t.add_callback(2598u, [&scene, &settings] {
        createUFOLights(scene);
        settings.animation.animateLights = true;
    });

    // Beam On
    t.add(2574u, 0.0f).to(1.0f).during(1000).onStep([&audio](float f) {
        audio.beamSoundInstance->setVolume(f * 7.0);
        return false;
    });


    // Beam Off
    t.add(3120u, 1.0f).to(0.0f).during(300).onStep([&audio](float f) {
        audio.beamSoundInstance->setVolume(f * 7.0);
        return false;
    });

    // UFO gone
    t.add_callback(3300u, [&settings] {
       settings.audio.ufoVolume = 0.0;
    });


    t.reset();

}
