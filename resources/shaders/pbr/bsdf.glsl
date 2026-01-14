#ifndef BSDF_H
#define BSDF_H

#include "diffuse.glsl"
#include "specular.glsl"
#include "normal.glsl"

// https://advances.realtimerendering.com/other/2016/naughty_dog/index.html
float microShadowNaughtyDog(float ao, float n_dot_l) {
    float aperture = 2.0 * ao * ao;// They use 2 * ao^2, but linear looks better imo
    return saturate(n_dot_l + aperture - 1.0);
}

struct BSDFParams {
// World Position
    vec3 P;
// Shading Normal
    vec3 N;
// Geometric Normal
    vec3 geoN;
// View Direction + Distance
    vec4 V;
    vec4 albedo;
    vec3 f0;
    float roughness;
    float metalness;
    float occlusion;
    float emissiveness;
};

vec3 bsdf(in BSDFParams p, vec3 L, vec3 radiance) {

    vec3 H = normalize(p.V.xyz + L);
    vec3 N = backsideNormal(p.N, p.geoN, L);

    // Clamped versions (For Specular and general shading)
    float n_dot_v = max(dot(N, p.V.xyz), 1e-5);
    float n_dot_l = max(dot(N, L), 0.0);
    float n_dot_h = max(dot(N, H), 0.0);
    float l_dot_h = max(dot(L, H), 0.0);

    // Unclamped version (Strictly required for Fujii/Oren-Nayar S-term)
    float l_dot_v = dot(L, p.V.xyz);


    // --- Specular Term (GGX) ---

    // Calculate F (Fresnel)
    vec3 F = fresnelSchlick(l_dot_h, p.f0);

    // Calculate Specular BRDF (D * V * F)
    vec3 specular = brdfSpecular(F, n_dot_l, n_dot_v, n_dot_h, p.roughness);

    // --- Diffuse Term ---

    // Calculate Diffuse Factor (Intensity)
    float diffuseFactor = brdfDiffuse(n_dot_l, n_dot_v, l_dot_v, l_dot_h, p.roughness);

    // Energy Conservation: Calculate kD
    // kS is equal to Fresnel (F). kD is the remaining energy.
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - p.metalness);

    vec3 diffuse = kD * p.albedo.rgb * diffuseFactor;

    float micro_shadow = microShadowNaughtyDog(p.occlusion, n_dot_l);
    radiance *= micro_shadow;

    // --- Emissive Term ---

    vec3 emissive = p.emissiveness * p.albedo.rgb;

    return (diffuse + specular) * radiance * n_dot_l + emissive;
}

#endif