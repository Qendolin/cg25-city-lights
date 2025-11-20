#include "common/descriptors_geom.glsl"
#include "common/descriptors_mat.glsl"

const int SHADOW_CASCADE_COUNT = 5;

struct SunLight {
    vec4 radiance;
    vec4 direction;
};

struct ShadowCascade {
    mat4 projectionView;
    float sampleBias;
    float sampleBiasClamp;
    float normalBias;
    float dimension;
};

layout (std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 projection;
    vec4 camera;
    SunLight sun;
    ShadowCascade[SHADOW_CASCADE_COUNT] cascades;
    vec4 ambient;
} uParams;