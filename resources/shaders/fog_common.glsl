// Must match C++ constants exactly
#define CLUSTER_DIM_X 32
#define CLUSTER_DIM_Y 18
#define CLUSTER_DIM_Z 24
#define FOG_MAX_DISTANCE 128.0
#define CLUSTER_LIGHT_STRIDE 64
#define MAX_LIGHTS_PER_CLUSTER (CLUSTER_LIGHT_STRIDE - 1)
#define CLUSTER_LIGHT_STRIDE_SHIFT 6 // 1 << 6 = 128

struct UberLight {
    vec3 position;
    float range;
    vec3 radiance;
    float coneAngleScale;
    vec2 direction; // octahedron encoded, allows us to store the point light size
    float pointSize; // controls the falloff near the light source
    float coneAngleOffset;
};
