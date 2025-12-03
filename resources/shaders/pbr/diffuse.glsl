#ifndef DIFFUSE_H
#define DIFFUSE_H

#include "../common/math.glsl"
#include "fresnel.glsl"

// 0 = Lambert (Cheapest, Standard)
// 1 = Fujii Oren-Nayar (Cheap, High Quality Roughness, Dusty look)
// 2 = Disney / Burley (Expensive, Physically Based, Retro-reflection)
#ifndef DIFFUSE_MODE
#define DIFFUSE_MODE 1
#endif

// Fujii's approximation of Oren-Nayar
// https://mimosa-pudica.net/improved-oren-nayar.html
float diffuseFujii(float n_dot_l, float n_dot_v, float l_dot_v, float roughness) {
    // Fujii uses sigma = roughness^2 (Variance)
    float sigma = roughness * roughness;

    // S term (l_dot_v must NOT be clamped)
    float s = l_dot_v - n_dot_l * n_dot_v;
    float t = s <= 0.0 ? 1.0 : max(n_dot_l, n_dot_v);

    float A = 1.0 / (PI + (PI_HALF - 2.0/3.0) * sigma);
    float B = sigma * A;

    return A + B * s / t;
}

// Disney's Diffuse Model (Burley 2012)
// https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
float diffuseBurley(float n_dot_l, float n_dot_v, float l_dot_h, float roughness) {
    float FD90 = 0.5 + 2.0 * roughness * l_dot_h * l_dot_h;

    float lightScatter = fresnelSchlick(n_dot_l, 1.0, FD90);
    float viewScatter  = fresnelSchlick(n_dot_v, 1.0, FD90);

    return INV_PI * lightScatter * viewScatter;
}

// Note: l_dot_v must be unclamped
float brdfDiffuse(
float n_dot_l,
float n_dot_v,
float l_dot_v,
float l_dot_h,
float roughness
) {
    #if DIFFUSE_MODE == 1
    return diffuseFujii(n_dot_l, n_dot_v, l_dot_v, roughness);
    #elif DIFFUSE_MODE == 2
    return diffuseBurley(n_dot_l, n_dot_v, l_dot_h, roughness);
    #else
    return INV_PI;
    #endif
}

#endif