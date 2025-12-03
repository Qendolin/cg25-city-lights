#ifndef FRESNEL_H
#define FRESNEL_H

vec3 fresnelSchlick(float cos_theta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cos_theta), 5.0);
}

float fresnelSchlick(float cos_theta, float f0, float f90)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cos_theta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(saturate(1.0 - cos_theta), 5.0);
}

#endif