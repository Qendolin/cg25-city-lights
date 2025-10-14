#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 in_position_ws;
layout (location = 1) in mat3 in_tbn;
layout (location = 4) in vec2 in_tex_coord;
layout (location = 5) flat in uint in_material;

layout (location = 0) out vec4 out_color;

#include "descriptors.glsl"

const float PI = 3.14159265359;
const uint NO_TEXTURE = 0xffff;

const int LIGHT_COUNT = 1;

void unpackUint16(in uint packed, out uint lower, out uint upper) {
    lower = packed & 0xffff;
    upper = (packed >> 16) & 0xffff;
}

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

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a_2 = a * a;
    float n_dot_h = max(dot(N, H), 0.0);
    float n_dot_h_2 = n_dot_h * n_dot_h;

    float nom = a_2;
    float denom = (n_dot_h_2 * (a_2 - 1.0) + 1.0);
    // when roughness is zero and N = H denom would be 0
    denom = PI * denom * denom + 5e-6;

    return nom / denom;
}

float geometrySchlickGGX(float n_dot_v, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = n_dot_v;
    float denom = n_dot_v * (1.0 - k) + k;

    return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    // + 5e-6 to prevent artifacts, value is from https://google.github.io/filament/Filament.html#materialsystem/specularbrdf:~:text=float%20NoV%20%3D%20abs(dot(n%2C%20v))%20%2B%201e%2D5%3B
    float n_dot_v = max(dot(N, V), 0.0) + 5e-6;
    float n_dot_l = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(n_dot_v, roughness);
    float ggx1 = geometrySchlickGGX(n_dot_l, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cos_theta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cos_theta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// https://advances.realtimerendering.com/other/2016/naughty_dog/index.html
float microShadowNaughtyDog(float ao, float n_dot_l) {
    float aperture = 2.0 * ao; // They use ao^2, but linear looks better imo
    return clamp(n_dot_l + aperture - 1.0, 0.0, 1.0);
}

void main() {
    Material material = uMaterialBuffer.materials[in_material];
    uint albedoTextureIndex, normalTextureIndex;
    unpackUint16(material.packedImageIndices0, albedoTextureIndex, normalTextureIndex);
    uint ormTextureIndex, unusedTextureIndex;
    unpackUint16(material.packedImageIndices1, ormTextureIndex, unusedTextureIndex);

    vec4 albedo = material.albedoFactors;
    if (albedoTextureIndex != NO_TEXTURE) {
        albedo *= texture(uTextures[nonuniformEXT(albedoTextureIndex)], in_tex_coord);
    }

    vec3 orm = vec3(1.0, material.rmnFactors.xy);
    if (ormTextureIndex != NO_TEXTURE) {
        orm *= texture(uTextures[nonuniformEXT(ormTextureIndex)], in_tex_coord).xyz;
    }
    float occlusion = orm.x;
    float roughness = orm.y;
    float metallic = orm.z;
    mat3 tbn = in_tbn;

    // tangent-space normal
    vec3 normal_ts = vec3(0.0, 0.0, 1.0);
    if (normalTextureIndex != NO_TEXTURE) {
        normal_ts.xy = texture(uTextures[nonuniformEXT(normalTextureIndex)], in_tex_coord).xy * 2.0 - 1.0;
        normal_ts.z = sqrt(1 - normal_ts.x * normal_ts.x - normal_ts.y * normal_ts.y);
        normal_ts = normalize(normal_ts * vec3(material.rmnFactors.z, material.rmnFactors.z, 1.0)); // increase intensity
        roughness = adjustRoughness(normal_ts, roughness);
    }

    vec3 N = transformNormal(tbn, normal_ts);
    vec3 P = in_position_ws;
    vec3 V = normalize(uScene.camera.xyz - P);
    vec3 R = reflect(-V, N);
    float n_dot_v = max(dot(N, V), 0.0);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        vec3 L = uScene.sun.direction.xyz;
        vec3 radiance = uScene.sun.radiance.xyz;

        // The half way vector
        vec3 H = normalize(V + L);

        // Use geo normal for surface facing away from light
        vec3 n = mix(N, tbn[2].xyz, clamp(-10 * dot(tbn[2].xyz, L), -0.5, 0.5) + 0.5);

        // Cook-Torrance BRDF
        float NDF = distributionGGX(n, H, roughness);
        float G = geometrySmith(n, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float n_dot_l = max(dot(n, L), 0.0);

        float micro_shadow = microShadowNaughtyDog(occlusion, n_dot_l);
        radiance *= micro_shadow;

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * n_dot_v * n_dot_l + 1e-5; // + 1e-5 to prevent divide by zero
        vec3 specular = numerator / denominator;

        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;

        // add to outgoing radiance Lo
        // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
        Lo += (kD * albedo.rgb / PI + specular) * radiance * n_dot_l;
    }

    vec3 ambient = vec3(1.0) * occlusion;
    ambient *= fresnelSchlickRoughness(n_dot_v, F0, roughness);
    ambient *= albedo.rgb;
    ambient *= 1.0 - metallic;

    vec3 color = ambient + Lo;
    // reinhard tonemaping
    color = color / (color + 1.0);
    // no gamma correction, swapchain uses srgb format
    out_color = vec4(color, 1.0);
}