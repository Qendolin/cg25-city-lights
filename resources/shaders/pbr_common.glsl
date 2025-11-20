#include "common/descriptors_geom.glsl"
#include "common/descriptors_mat.glsl"
#include "common/descriptors_light.glsl"

const int SHADOW_CASCADE_COUNT = 5;

struct ShadowCascade {
    mat4 projectionView;
    float sampleBias;
    float sampleBiasClamp;
    float normalBias;
    float dimension;
};

// FIXME: Inline uniform block is too large!
layout (std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 projection;
    vec4 camera;
    DirectionalLight sun;
    ShadowCascade[SHADOW_CASCADE_COUNT] cascades;
    vec4 ambient;
} uParams;