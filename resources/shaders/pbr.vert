#version 460

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec4 in_tangent;
layout (location = 3) in vec2 in_tex_coord;

layout (location = 0) out vec3 out_position_ws;
layout (location = 1) out mat3 out_tbn; // a mat3 uses 3 locations
layout (location = 4) out vec2 out_tex_coord;
layout (location = 5) flat out uint out_material;

#include "descriptors.glsl"

void main() {
    Section section = uSectionBuffer.sections[gl_DrawID];
    Instance instance = uInstanceBuffer.instances[section.instance];

    vec4 position_ws = instance.transform * vec4(in_position, 1.0);
    gl_Position = uScene.projection * uScene.view * position_ws;
    out_position_ws = position_ws.xyz;
    out_tex_coord = in_tex_coord;
    out_material = section.material;

    // Doesn't support non-uniform scaling
    mat3 normal_matrix = mat3(instance.transform);
    vec3 T = normalize(normal_matrix * in_tangent.xyz);
    vec3 N = normalize(normal_matrix * in_normal);
    vec3 bitangent = cross(in_normal, in_tangent.xyz) * in_tangent.w;
    vec3 B = normalize(normal_matrix * bitangent);
    out_tbn = mat3(T, B, N);
}