#version 460

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec4 in_tangent;
layout (location = 3) in vec2 in_tex_coord;

layout (location = 0) out vec3 out_position_ws;
layout (location = 1) out mat3 out_tbn; // a mat3 uses 3 locations
layout (location = 4) out vec2 out_tex_coord;
layout (location = 5) flat out uint out_material;
layout (location = 6) out vec3 out_shadow_position_ndc;

#include "pbr_common.glsl"

vec3 shadowSamplePosition(in mat4 projectionView, vec3 position, vec3 normal) {
    vec4 shadow_ws = vec4(position, 1.0);

    // https://web.archive.org/web/20160602232409if_/http://www.dissidentlogic.com/old/images/NormalOffsetShadows/GDC_Poster_NormalOffset.png
    // https://github.com/TheRealMJP/Shadows/blob/8bcc4a4bbe232d5f17eda5907b5a7b5425c54430/Shadows/Mesh.hlsl#L716C8-L716C26
    // https://c0de517e.blogspot.com/2011/05/shadowmap-bias-notes.html
    float n_dot_l = dot(normal, uParams.sun.direction.xyz);
    vec3 offset = uParams.sun.normalBias * (1.0 - n_dot_l) * normal;
    shadow_ws.xyz += offset;

    vec4 shadow_ndc = projectionView * shadow_ws;

    // Usually this divide is required, but the shadow projection is orthogonal, so we can omit it.
    // If this wasn't the case we also couldn't do it in the vs, because vs outputs need to be linear in order to be
    // interpolated properly.
    // shadow_ndc.xyz / shadow_ndc.w;
    return shadow_ndc.xyz;
}

void main() {
    Section section = uSectionBuffer.sections[gl_DrawID];
    Instance instance = uInstanceBuffer.instances[section.instance];

    vec4 position_ws = instance.transform * vec4(in_position, 1.0);
    gl_Position = uParams.projection * uParams.view * position_ws;
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

    out_shadow_position_ndc = shadowSamplePosition(
        uParams.sun.projectionView,
        out_position_ws,
        N);
}