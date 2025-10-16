#include "common/descriptors_geom.glsl"
#include "common/descriptors_mat.glsl"

struct SunLight {
    mat4 projectionView;
    vec4 radiance;
    vec4 direction;
    float sampleBias;
    float sampleBiasClamp;
    float normalBias;
    float pad0;
};

layout (std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 projection;
    vec4 camera;
    SunLight sun;
} uParams;