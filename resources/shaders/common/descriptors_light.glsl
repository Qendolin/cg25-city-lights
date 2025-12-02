#define LIGHT_TILE_BUFFER_STRIDE_SHIFT 8
// 256
#define LIGHT_TILE_BUFFER_STRIDE (1 << LIGHT_TILE_BUFFER_STRIDE_SHIFT)
#define LIGHT_TILE_MAX_LIGHTS (LIGHT_TILE_BUFFER_STRIDE - 1)

#define LIGHT_TILE_SHIFT 4
// 16
#define LIGHT_TILE_SIZE (1 << LIGHT_TILE_SHIFT)
#define LIGHT_TILE_SIZE_2 (LIGHT_TILE_SIZE * LIGHT_TILE_SIZE)

struct DirectionalLight {
    vec4 radiance;
    vec4 direction;
};

// Size is 4 * vec3 = 48 bytes with an alignment of 16
// For point lights scale is 0 and offset is 1
struct UberLight {
    vec3 position;
    float range;

    // color * intensity
    vec3 radiance;
    float coneAngleScale;

    vec2 direction; // octahedron encoded, allows us to store the point light size
    float pointSize; // controls the falloff near the light source
    float coneAngleOffset;
};

layout (std430, set = 0, binding = 4) readonly buffer UberLightBuffer {
    UberLight uUberLightBuffer[];
};
