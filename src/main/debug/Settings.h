#pragma once
#include <array>

#include "../entity/Light.h"

class ShadowCaster;

struct Settings {
    static constexpr int SHADOW_CASCADE_COUNT = 5;
    DirectionalLight sun = {.elevation = 40.0f, .azimuth = 10.0f, .color = glm::vec3{1.0f, 1.0f, 1.0f}, .power = 15.0f};
    struct Sky {
        float exposure = 1.49f;
        glm::vec3 tint = glm::vec3(1.0f);
    } sky;
    struct Shadow {
        float extrusionBias = -0.5f;
        float normalBias = 7.0f;
        float sampleBias = 0.1f;
        float sampleBiasClamp = 0.02f;
        float depthBiasConstant = 0.0f;
        float depthBiasSlope = -2.5f;
        float depthBiasClamp = 0.0f;

        void applyTo(ShadowCaster &caster) const;
    };
    std::array<Shadow, SHADOW_CASCADE_COUNT> shadowCascades;
    struct ShadowCascade {
        float lambda = 0.9f;
        float distance = 64.0f;
        const int resolution = 2048;
        bool visualize = false;
        bool update = true;
    } shadowCascade;

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
        glm::vec3 ambient = glm::vec3(0.28f, 0.315, 0.385);
        bool enableFrustumCulling = true;
        bool pauseFrustumCulling = false;
        bool whiteWorld = false;
        bool lightDensity = false;
        float lightRangeFactor = 1.0f;
        bool asyncCompute = true;
    } rendering;
    struct SSAO {
        bool update = true;
        bool halfResolution = true;
        bool bentNormals = true;
        int slices = 3;
        int samples = 6;
        float radius = 2.0f;
        float exponent = 2.0f;
        float bias = 0.0f;
        float filterSharpness = 20.0f;
    } ssao;
    struct Blob {
        bool render = false;
    } blob;

    Settings() {
        shadowCascades[0] = {
            .extrusionBias = -0.5f,
            .normalBias = 20.0f,
            .sampleBias = 0.01f,
            .sampleBiasClamp = 0.01f,
            .depthBiasConstant = 0.0f,
            .depthBiasSlope = -1.0f,
        };
    }
};

