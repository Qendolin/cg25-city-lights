#ifndef NORMAL_H
#define NORMAL_H

vec3 transformNormal(mat3 tbn, vec3 tangent_normal) {
    return normalize(tbn * tangent_normal);
}

// Based on https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/course-notes-moving-frostbite-to-pbr-v32.pdf page 92
float adjustRoughness(vec3 tangent_normal, float roughness) {
    float r = length(tangent_normal);
    if (r < 1.0) {
        float kappa = (3.0 * r - r * r * r) / (1.0 - r * r);
        float variance = 1.0 / kappa;
        // Why is it ok for the roughness to be > 1 ?
        return sqrt(roughness * roughness + variance);
    }
    return roughness;
}

vec3 backsideNormal(vec3 texture_normal, vec3 geometry_normal, vec3 light_dir) {
    // Use geo normal for surface facing away from light
    return mix(texture_normal, geometry_normal, clamp(-10 * dot(geometry_normal, light_dir), -0.5, 0.5) + 0.5);
}

#endif