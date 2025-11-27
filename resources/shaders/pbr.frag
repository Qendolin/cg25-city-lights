#version 460

#extension GL_EXT_nonuniform_qualifier: require

layout (early_fragment_tests) in;

#include "pbr_common.glsl"

layout (location = 0) in vec3 in_position_ws;
layout (location = 1) in mat3 in_tbn;
layout (location = 4) in vec2 in_tex_coord;
layout (location = 5) flat in uint in_material;
layout (location = 6) in vec3 in_shadow_position_ndc[SHADOW_CASCADE_COUNT];

layout (location = 0) out vec4 out_color;

layout (set = 1, binding = 1) uniform sampler2DShadow uSunShadowMaps[SHADOW_CASCADE_COUNT];
layout (set = 1, binding = 3) uniform sampler2D uAmbientOcclusion;

const float PI = 3.14159265359;
const float INV_PI = 1.0 / 3.14159265359;
const vec3 LIGHT_EPSILON = vec3(1.0 / 255.0);
const uint NO_TEXTURE = 0xffff;
const uint FLAG_SHADOW_CASCADES = 0x1;
const uint FLAG_WHITE_WORLD = 0x2;

const vec3 CASCADE_DEBUG_COLORS[6] = {
vec3(1.0, 0.0, 0.0),
vec3(1.0, 1.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(0.0, 1.0, 1.0),
vec3(0.0, 0.0, 1.0),
vec3(1.0, 1.0, 1.0)
};

const vec2 POISSON[9] = vec2[](
vec2(-0.75, -0.25), vec2(-0.25, -0.75), vec2( 0.25, -0.5),
vec2(-0.5,   0.25), vec2( 0.0,   0.0 ), vec2( 0.5,   0.25),
vec2(-0.25,  0.5 ), vec2( 0.75,  0.25), vec2( 0.25,  0.75)
);

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
    float aperture = 2.0 * ao * ao; // They use 2 * ao^2, but linear looks better imo
    return clamp(n_dot_l + aperture - 1.0, 0.0, 1.0);
}

float sampleShadowGpuGems(vec3 P_shadow_ndc, float n_dot_l, int index) {
    ShadowCascade cascade = uShadowCascades.cascades[index];
    vec2 texel_size = vec2(1.0) / textureSize(uSunShadowMaps[index], 0).xy;
    // z is seperate because we are using 0..1 depth
    vec3 shadow_uvz = vec3(P_shadow_ndc.xy * 0.5 + 0.5, P_shadow_ndc.z);

    float bias = cascade.sampleBias * texel_size.x * tan(acos(n_dot_l));
    bias = clamp(bias, 0.0, cascade.sampleBiasClamp * texel_size.x);

    // GPU Gems 1 / Chapter 11.4
    vec2 offset = vec2(fract(gl_FragCoord.x * 0.5) > 0.25, fract(gl_FragCoord.y * 0.5) > 0.25); // mod
    offset.y += offset.x; // y ^= x in floating point
    if (offset.y > 1.1) offset.y = 0;
    float shadow = 0.0;
    // + bias instead of - bias becase we are using reversed depth and the GL_GEQUAL compare mode.
    shadow += texture(uSunShadowMaps[index], vec3(shadow_uvz.xy + (offset + vec2(-1.5, 0.5)) * texel_size, shadow_uvz.z + bias));
    shadow += texture(uSunShadowMaps[index], vec3(shadow_uvz.xy + (offset + vec2(0.5, 0.5)) * texel_size, shadow_uvz.z + bias));
    shadow += texture(uSunShadowMaps[index], vec3(shadow_uvz.xy + (offset + vec2(-1.5, -1.5)) * texel_size, shadow_uvz.z + bias));
    shadow += texture(uSunShadowMaps[index], vec3(shadow_uvz.xy + (offset + vec2(0.5, -1.5)) * texel_size, shadow_uvz.z + bias));
    shadow *= 0.25;

    return shadow;
}

mat2 rotate(float a) {
    float s = sin(a), c = cos(a);
    return mat2(c,-s,s,c);
}

// Higher quality than GpuGems
float sampleShadowPoisson(vec3 P_shadow_ndc, float n_dot_l, int index, float distance_vs) {
    ShadowCascade cascade = uShadowCascades.cascades[index];
    vec2 texel_size = vec2(1.0f) / textureSize(uSunShadowMaps[index], 0).xy;
    // z is seperate because we are using 0..1 depth
    vec3 shadow_uvz = vec3(P_shadow_ndc.xy * 0.5f + 0.5f, P_shadow_ndc.z);

    // tan(acos(x)) = sqrt(1-x^2)/x
    n_dot_l = max(n_dot_l, 1e-5f);
    float bias = cascade.sampleBias * texel_size.x * sqrt(1.0f - n_dot_l * n_dot_l) / n_dot_l;
    bias = clamp(bias, 0.0f, cascade.sampleBiasClamp * texel_size.x);

    float split = uShadowCascades.cascades[index].splitDistance;
    float blend_start = split * 0.5f;
    float blend_end   = split;
    float kernel_scale = 1.0f + 1.0f * clamp((distance_vs - blend_start) / (blend_end - blend_start), 0.0f, 1.0f);

    float result = 0.0f;
    for(int i = 0; i < 9; ++i) {
        vec2 offset = kernel_scale * POISSON[i] * texel_size * 2.0f;
        result += texture(uSunShadowMaps[index], vec3(shadow_uvz.xy + offset, shadow_uvz.z + bias));
    }
    return result / 9.0f;
}

vec3 backsideNormal(vec3 texture_normal, vec3 geometry_normal, vec3 light_dir) {
    // Use geo normal for surface facing away from light
    return mix(texture_normal, geometry_normal, clamp(-10 * dot(geometry_normal, light_dir), -0.5, 0.5) + 0.5);
}

struct BSDFParams {
    vec4 albedo;
    vec3 f0;
    float roughness;
    float metalness;
    float occlusion;
};

vec3 bsdf(vec3 light_dir, vec3 view_dir, vec3 texture_normal, vec3 geometry_normal, in BSDFParams p, vec3 radiance) {
    // The half way vector
    vec3 halfway_dir = normalize(view_dir + light_dir);
    vec3 normal = backsideNormal(texture_normal, geometry_normal, light_dir);

    // Cook-Torrance BRDF
    float NDF = distributionGGX(normal, halfway_dir, p.roughness);
    float G = geometrySmith(normal, view_dir, light_dir, p.roughness);
    vec3 F = fresnelSchlick(max(dot(halfway_dir, view_dir), 0.0), p.f0);

    float n_dot_l = max(dot(normal, light_dir), 0.0);
    float n_dot_v = max(dot(normal, view_dir), 0.0);

    float micro_shadow = microShadowNaughtyDog(p.occlusion, n_dot_l);
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
    kD *= 1.0 - p.metalness;

    // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    return (kD * p.albedo.rgb * INV_PI + specular) * radiance * n_dot_l;
}

// https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#:~:text=Reference%20code
float spotFalloff(in SpotLight light, vec3 normalized_light_vector) {
    float cd = dot(light.direction.xyz, normalized_light_vector);
    float angularAttenuation = clamp(cd * light.coneAngleScale + light.coneAngleOffset, 0.0, 1.0);
    return angularAttenuation * angularAttenuation;
}

void main() {
    Material material = uMaterialBuffer.materials[in_material];
    BSDFParams bsdf_params;
    uint albedoTextureIndex, normalTextureIndex;
    unpackUint16(material.packedImageIndices0, albedoTextureIndex, normalTextureIndex);
    uint ormTextureIndex, unusedTextureIndex;
    unpackUint16(material.packedImageIndices1, ormTextureIndex, unusedTextureIndex);

    if ((cParams.flags & FLAG_WHITE_WORLD) != 0x0) {
        bsdf_params.albedo.xyz = vec3(1.0f);
    } else {
        bsdf_params.albedo = material.albedoFactors;
        if (albedoTextureIndex != NO_TEXTURE) {
            bsdf_params.albedo *= texture(uTextures[nonuniformEXT(albedoTextureIndex)], in_tex_coord);
        }
    }

    vec3 orm = vec3(1.0, material.rmnFactors.xy);
    if (ormTextureIndex != NO_TEXTURE) {
        orm *= texture(uTextures[nonuniformEXT(ormTextureIndex)], in_tex_coord).xyz;
    }
    bsdf_params.occlusion = orm.x;
    bsdf_params.roughness = orm.y;
    bsdf_params.metalness = orm.z;
    mat3 tbn = in_tbn;

    // tangent-space normal
    vec3 normal_ts = vec3(0.0, 0.0, 1.0);
    if (normalTextureIndex != NO_TEXTURE) {
        normal_ts.xy = texture(uTextures[nonuniformEXT(normalTextureIndex)], in_tex_coord).xy * 2.0 - 1.0;
        normal_ts.z = sqrt(1 - normal_ts.x * normal_ts.x - normal_ts.y * normal_ts.y);
        normal_ts = normalize(normal_ts * vec3(material.rmnFactors.z, material.rmnFactors.z, 1.0)); // increase intensity
        bsdf_params.roughness = adjustRoughness(normal_ts, bsdf_params.roughness);
    }

    vec3 geometry_normal = normalize(tbn[2].xyz);
    vec3 texture_normal = transformNormal(tbn, normal_ts);
    vec3 position = in_position_ws;
    vec3 view_dir = normalize(uParams.camera.xyz - position);
    float n_dot_v = max(dot(texture_normal, view_dir), 0.0);
    float distance_vs = distance(uParams.camera.xyz, position);

    bsdf_params.f0 = vec3(0.04);
    bsdf_params.f0 = mix(bsdf_params.f0, bsdf_params.albedo.rgb, bsdf_params.metalness);

    int shadow_index = SHADOW_CASCADE_COUNT-1;
    for (int i = SHADOW_CASCADE_COUNT - 1; i >= 0; i--) {
        if (distance_vs < uShadowCascades.cascades[i].splitDistance) {
            shadow_index = i;
        }
    }

    vec3 Lo = vec3(0.0);

    // directional light
    {
        float shadow = sampleShadowPoisson(in_shadow_position_ndc[shadow_index], dot(geometry_normal, uParams.sun.direction.xyz), shadow_index, distance_vs);
        vec3 radiance = uParams.sun.radiance.xyz * shadow;
        if (any(greaterThan(radiance, LIGHT_EPSILON))) {
            Lo += bsdf(uParams.sun.direction.xyz, view_dir, texture_normal, geometry_normal, bsdf_params, radiance);
        }
    }

    // spot light
    for (int i = 0; i < uSpotLightBuffer.lights.length(); i++) {
        SpotLight light = uSpotLightBuffer.lights[i];
        vec3 light_vec = light.position.xyz - position;
        float d = length(light_vec);
        vec3 light_dir = light_vec / d;
        vec3 radiance = light.radiance.xyz / (d * d);
        radiance *= spotFalloff(light, -light_dir);
        if (any(greaterThan(radiance, LIGHT_EPSILON))) {
            Lo += bsdf(light_dir, view_dir, texture_normal, geometry_normal, bsdf_params, radiance);
        }
    }

    // point light
    for (int i = 0; i < uPointLightBuffer.lights.length(); i++) {
        PointLight light = uPointLightBuffer.lights[i];
        vec3 light_vec = light.position.xyz - position;
        float d = length(light_vec);
        vec3 light_dir = light_vec / d;
        vec3 radiance = light.radiance.xyz / (d * d);
        if (any(greaterThan(radiance, LIGHT_EPSILON))) {
            Lo += bsdf(light_dir, view_dir, texture_normal, geometry_normal, bsdf_params, radiance);
        }
    }

    vec2 screen_uv = vec2(gl_FragCoord.xy) * uParams.viewport.zw;
    float ambient_occlusion = textureLod(uAmbientOcclusion, screen_uv, 0).x;

    vec3 ambient = uParams.ambient.rgb * bsdf_params.occlusion * ambient_occlusion;
    ambient *= fresnelSchlickRoughness(n_dot_v, bsdf_params.f0, bsdf_params.roughness);
    ambient *= bsdf_params.albedo.rgb;
    ambient *= 1.0 - bsdf_params.metalness;

    vec3 color = ambient + Lo;
    out_color = vec4(color, 1.0);

    if ((cParams.flags & FLAG_SHADOW_CASCADES) != 0x0) {
        out_color.rgb = mix(out_color.rgb, CASCADE_DEBUG_COLORS[shadow_index], 0.3f);
    }
}