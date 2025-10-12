
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

layout (std140, set = 1, binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 projection;
    vec4 camera;
} uScene;