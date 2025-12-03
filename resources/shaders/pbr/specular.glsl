#ifndef GGX_IMPROVED_H
#define GGX_IMPROVED_H

#include "../common/math.glsl"

// 0 = Basic (Separable / Epic Games approx)
// 1 = Improved (Height-Correlated / Heitz)
#ifndef SPECULAR_MODE
#define SPECULAR_MODE 1
#endif

// Trowbridge-Reitz GGX Normal Distribution Function
// Note: Uses alpha (roughness^2)
float D_GGX(float n_dot_h, float alpha) {
    float a2 = alpha * alpha;
    float f = (n_dot_h * n_dot_h) * (a2 - 1.0) + 1.0;
    return a2 / (PI * f * f);
}

// Mode 0: Basic / Separable

float geometrySchlickGGX(float n_dot_v, float roughness) {
    // Note: This specific k mapping is for direct lighting only!
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;

    return nom / denom;
}

float geometrySmith(float n_dot_v, float n_dot_l, float roughness) {
    float ggx2 = geometrySchlickGGX(n_dot_v, roughness);
    float ggx1 = geometrySchlickGGX(n_dot_l, roughness);
    return ggx1 * ggx2;
}

// Mode 1: Improved / Height-Correlated (New Way)

// Heitz 2014 - Height Correlated Visibility
// Note: Uses alpha (roughness^2)
float V_SmithGGXCorrelated(float n_dot_v, float n_dot_l, float alpha) {
    float a2 = alpha * alpha;
    float GGXV = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0 - a2) + a2);
    float GGXL = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

vec3 brdfSpecular(
vec3 F,
float n_dot_l,
float n_dot_v,
float n_dot_h,
float roughness
) {
    // Create Alpha (Roughness^2) for the NDF and Correlated Visibility
    float alpha = roughness * roughness;
    float D = D_GGX(n_dot_h, alpha);

    #if SPECULAR_MODE == 0
        // Calculates G independently
    float G = geometrySmith(n_dot_v, n_dot_l, roughness);

    // We must manually divide by the standard PBR denominator
    float denominator = 4.0 * n_dot_v * n_dot_l + 1e-5;

    return (D * G * F) / denominator;
    #elif SPECULAR_MODE == 1
    // V combines the Geometry term and the (1 / 4*NoL*NoV) denominator
    float V = V_SmithGGXCorrelated(n_dot_v, n_dot_l, alpha);

    return D * V * F;
    #endif
}

#endif