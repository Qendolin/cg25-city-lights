#ifndef SHADOW_H
#define SHADOW_H

const vec2 SHADOW_POISSON_DISK[9] = vec2[](
vec2(-0.75, -0.25), vec2(-0.25, -0.75), vec2(0.25, -0.5),
vec2(-0.5, 0.25), vec2(0.0, 0.0), vec2(0.5, 0.25),
vec2(-0.25, 0.5), vec2(0.75, 0.25), vec2(0.25, 0.75)
);

float sampleShadowGpuGems(in ShadowCascade cascade, in sampler2DShadow shadow_map, vec3 P_shadow_ndc, float n_dot_l) {
    vec2 texel_size = vec2(1.0) / textureSize(shadow_map, 0).xy;
    // z is seperate because we are using 0..1 depth
    vec3 shadow_uvz = vec3(P_shadow_ndc.xy * 0.5 + 0.5, P_shadow_ndc.z);

    float bias = cascade.sampleBias * texel_size.x * tan(acos(n_dot_l));
    bias = clamp(bias, 0.0, cascade.sampleBiasClamp * texel_size.x);

    // GPU Gems 1 / Chapter 11.4
    vec2 offset = vec2(fract(gl_FragCoord.x * 0.5) > 0.25, fract(gl_FragCoord.y * 0.5) > 0.25);// mod
    offset.y += offset.x;// y ^= x in floating point
    if (offset.y > 1.1) offset.y = 0;
    float shadow = 0.0;
    // + bias instead of - bias becase we are using reversed depth and the GL_GEQUAL compare mode.
    shadow += texture(shadow_map, vec3(shadow_uvz.xy + (offset + vec2(-1.5, 0.5)) * texel_size, shadow_uvz.z + bias));
    shadow += texture(shadow_map, vec3(shadow_uvz.xy + (offset + vec2(0.5, 0.5)) * texel_size, shadow_uvz.z + bias));
    shadow += texture(shadow_map, vec3(shadow_uvz.xy + (offset + vec2(-1.5, -1.5)) * texel_size, shadow_uvz.z + bias));
    shadow += texture(shadow_map, vec3(shadow_uvz.xy + (offset + vec2(0.5, -1.5)) * texel_size, shadow_uvz.z + bias));
    shadow *= 0.25;

    return shadow;
}

// Higher quality than GpuGems
float sampleShadowPoisson(in ShadowCascade cascade, in sampler2DShadow shadow_map, vec3 P_shadow_ndc, float n_dot_l, float distance_vs) {
    vec2 texel_size = vec2(1.0f) / textureSize(shadow_map, 0).xy;
    // z is seperate because we are using 0..1 depth
    vec3 shadow_uvz = vec3(P_shadow_ndc.xy * 0.5f + 0.5f, P_shadow_ndc.z);

    // tan(acos(x)) = sqrt(1-x^2)/x
    n_dot_l = max(n_dot_l, 1e-5f);
    float bias = cascade.sampleBias * texel_size.x * sqrt(1.0f - n_dot_l * n_dot_l) / n_dot_l;
    bias = clamp(bias, 0.0f, cascade.sampleBiasClamp * texel_size.x);

    float split = cascade.splitDistance;
    float blend_start = split * 0.5f;
    float blend_end   = split;
    float kernel_scale = 1.0f + 1.0f * saturate((distance_vs - blend_start) / (blend_end - blend_start));

    float result = 0.0f;
    for (int i = 0; i < 9; ++i) {
        vec2 offset = kernel_scale * SHADOW_POISSON_DISK[i] * texel_size * 2.0f;
        result += texture(shadow_map, vec3(shadow_uvz.xy + offset, shadow_uvz.z + bias));
    }
    return result / 9.0f;
}

#endif