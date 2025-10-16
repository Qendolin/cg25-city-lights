struct Instance {
    mat4 transform;
};

struct Section {
    uint instance;
    uint material;
};

layout(std430, set = 0, binding = 0) readonly buffer SectionBuffer {
    Section sections[];
} uSectionBuffer;

layout(std430, set = 0, binding = 1) readonly buffer InstanceBuffer {
    Instance instances[];
} uInstanceBuffer;
