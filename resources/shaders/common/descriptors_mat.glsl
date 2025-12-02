struct Material {
    vec4 albedoFactors;
    vec4 rmnFactors; // roughness, metalness, normal strength
    // lo=albedo, hi=normal
    uint packedImageIndices0;
    // lo=orm, hi=unused
    uint packedImageIndices1;
};

layout (std430, set = 0, binding = 2) readonly buffer MaterialBuffer {
    Material uMaterialBuffer[];
};

layout (set = 0, binding = 3) uniform sampler2D uTextures[];