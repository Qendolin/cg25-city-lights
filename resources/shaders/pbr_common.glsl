#include "common/descriptors_geom.glsl"
#include "common/descriptors_mat.glsl"
#include "common/descriptors_light.glsl"

const int SHADOW_CASCADE_COUNT = 5;

struct ShadowCascade {
    mat4 projectionView;
    float sampleBias;
    float sampleBiasClamp;
    float normalBias;
    float splitDistance;
};

layout (std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 projection;
    vec4 camera;
    vec4 viewport; // viewport size and inverse viewport size
    DirectionalLight sun;
    vec4 ambient;
} uParams;

layout (std140, set = 1, binding = 2) uniform ShadowCascadesUniforms {
    ShadowCascade[SHADOW_CASCADE_COUNT] cascades;
} uShadowCascades;

layout (push_constant) uniform ShaderPushConstants
{
    uint flags;
} cParams;