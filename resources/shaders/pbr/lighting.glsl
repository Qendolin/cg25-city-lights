#ifndef LIGHTING_H
#define LIGHTING_H

#include "bsdf.glsl"
#include "../common/math.glsl"

const vec3 LIGHT_EPSILON = vec3(0.001);

uint calculateTileLightBaseIndex(uvec2 frag_coord) {
    uvec2 light_tile_co = frag_coord >> LIGHT_TILE_SHIFT;
    // basically ceil division
    uint light_tiles_x = (uint(uParams.viewport.x) + LIGHT_TILE_SIZE - 1) >> LIGHT_TILE_SHIFT;
    uint light_tile_index = light_tile_co.x + light_tile_co.y * light_tiles_x;
    return light_tile_index << LIGHT_TILE_BUFFER_STRIDE_SHIFT;
}

float getDistanceAttenuation(float distance, float radius, float pointSize) {
    float s = distance / radius;
    if (s >= 1.0) return 0.0;

    float s2 = s * s;
    return saturate((1.0 - s2) / (distance * distance + pointSize));
}

vec3 evaluateUberLight(in UberLight light, in BSDFParams bsdf_params) {
    vec3 to_light_vec = light.position - bsdf_params.P;
    float dist = length(to_light_vec);
    vec3 to_light_dir = to_light_vec / dist;

    // inverse square law
    float dist_attenuation = getDistanceAttenuation(dist, light.range, light.pointSize);

    // for point lights scale is 0 and offset is 1 which allows uniform control flow
    vec3 light_dir = octahedronDecode(light.direction);
    float spot_cos = dot(-light_dir, to_light_dir);
    float spot_attenuation = saturate(spot_cos * light.coneAngleScale + light.coneAngleOffset);
    spot_attenuation *= spot_attenuation;

    vec3 radiance = light.radiance * dist_attenuation * spot_attenuation - LIGHT_EPSILON;

    // hoping the gpu can skip the expensive bsdf call
    if (any(greaterThan(radiance, vec3(0.0f)))) {
        return bsdf(bsdf_params, to_light_dir, radiance);
    } else {
        return vec3(0.0f);
    }
}

#endif