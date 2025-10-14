
// Types

struct Instance {
    mat4 transform;
};

struct Section {
    uint instance;
    uint material;
};

struct Material {
    vec4 albedoFactors;
    vec4 mrnFactors; // metalness, roughness, normal strength
    // lo=albedo, hi=normal
    uint packedImageIndices0;
    // lo=omr, hi=unused
    uint packedImageIndices1;
};

// Descriptor Sets

layout(std430, set = 0, binding = 0) readonly buffer SectionBuffer {
    Section sections[];
} uSectionBuffer;

layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    Instance instances[];
} uInstanceBuffer;

layout (std430, set = 0, binding = 2) readonly buffer MaterialBuffer {
    Material materials[];
} uMaterialBuffer;

layout (set = 0, binding = 3) uniform sampler2D uTextures[];

layout (std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 projection;
    vec4 camera;
} uScene;