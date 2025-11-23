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

layout (std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 projection;
    vec4 camera;
    DirectionalLight sun;
    vec4 ambient;
} uParams;

layout (std140, set = 1, binding = 2) uniform ShadowCascadesUniforms {
    ShadowCascade[SHADOW_CASCADE_COUNT] cascades;
} uShadowCascades;