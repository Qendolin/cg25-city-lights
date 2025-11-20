struct DirectionalLight {
    vec4 radiance;
    vec4 direction;
};

struct PointLight {
    vec4 radiance;
    vec4 position;
};

struct SpotLight {
    vec4 radiance;
    vec4 position;
    vec4 direction;
    float coneAngleScale;
    float coneAngleOffset;
    float pad0;
    float pad1;
};

layout (std430, set = 0, binding = 4) readonly buffer PointLightBuffer {
    PointLight lights[];
} uPointLightBuffer;

layout (std430, set = 0, binding = 5) readonly buffer SpotLightBuffer {
    SpotLight lights[];
} uSpotLightBuffer;

