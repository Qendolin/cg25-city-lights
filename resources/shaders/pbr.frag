#version 460

#extension GL_EXT_nonuniform_qualifier: require

layout (early_fragment_tests) in;

#include "common/math.glsl"
#include "pbr_common.glsl"

#include "pbr/bsdf.glsl"
#include "pbr/lighting.glsl"
#include "pbr/normal.glsl"
#include "pbr/shadow.glsl"

#include "colormap/jet.glsl"

layout (location = 0) in vec3 in_position_ws;
layout (location = 1) in mat3 in_tbn;
layout (location = 4) in vec2 in_tex_coord;
layout (location = 5) flat in uint in_material;
layout (location = 6) in vec3 in_shadow_position_ndc[SHADOW_CASCADE_COUNT];

layout (location = 0) out vec4 out_color;

layout (set = 1, binding = 1) uniform sampler2DShadow uSunShadowMaps[SHADOW_CASCADE_COUNT];
layout (set = 1, binding = 3) uniform sampler2D uAmbientOcclusion;

layout (std430, set = 1, binding = 4) readonly buffer TileLightIndicesBuffer {
    uint uTileLightIndices[];
};

const uint NO_TEXTURE = 0xffff;
const uint FLAG_SHADOW_CASCADES = 0x1;
const uint FLAG_WHITE_WORLD = 0x2;
const uint FLAG_LIGHT_DENSITY = 0x4;

const vec3 CASCADE_DEBUG_COLORS[6] = {
vec3(1.0, 0.0, 0.0),
vec3(1.0, 1.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(0.0, 1.0, 1.0),
vec3(0.0, 0.0, 1.0),
vec3(1.0, 1.0, 1.0)
};

void unpackUint16(in uint packed, out uint lower, out uint upper) {
    lower = packed & 0xffffu;
    upper = (packed >> 16) & 0xffffu;
}

// Approximates single-bounce ambient occlusion to multi-bounce ambient occlusion
// https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf#page=78
vec3 gtaoMultibounce(float visibility, vec3 base_color) {
    vec3 a = 2.0404 * base_color - 0.3324;
    vec3 b = -4.7951 * base_color + 0.6417;
    vec3 c = 2.7552 * base_color + 0.6903;
    vec3 x = vec3(visibility);
    return max(x, ((x * a + b) * x + c) * x);
}

void main() {
    Material material = uMaterialBuffer[in_material];
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

    // tangent-space normal
    vec3 normal_ts = vec3(0.0, 0.0, 1.0);
    if (normalTextureIndex != NO_TEXTURE) {
        normal_ts.xy = texture(uTextures[nonuniformEXT(normalTextureIndex)], in_tex_coord).xy * 2.0 - 1.0;
        normal_ts.z = sqrt(1 - normal_ts.x * normal_ts.x - normal_ts.y * normal_ts.y);
        // increase intensity
        float strength = material.rmnFactors.z;
        normal_ts = normalize(normal_ts * vec3(strength, strength, 1.0));
        bsdf_params.roughness = adjustRoughness(normal_ts, bsdf_params.roughness);
    }

    bsdf_params.P = in_position_ws;
    bsdf_params.V.xyz = uParams.camera.xyz - bsdf_params.P;
    bsdf_params.V.w = length(bsdf_params.V.xyz);
    bsdf_params.V.xyz /= bsdf_params.V.w;
    bsdf_params.N = transformNormal(in_tbn, normal_ts);
    bsdf_params.geoN = normalize(in_tbn[2].xyz);

    bsdf_params.f0 = vec3(0.04);
    bsdf_params.f0 = mix(bsdf_params.f0, bsdf_params.albedo.rgb, bsdf_params.metalness);

    int shadow_index = SHADOW_CASCADE_COUNT-1;
    {
        // Project into light space
        float lx = dot(bsdf_params.P, uParams.sun.right.xyz);
        float ly = dot(bsdf_params.P, uParams.sun.up.xyz);

        for (int i = 0; i < SHADOW_CASCADE_COUNT; ++i) {
            if (lx >= uShadowCascades[i].boundsMin.x && ly >= uShadowCascades[i].boundsMin.y
            && lx <= uShadowCascades[i].boundsMax.x && ly <= uShadowCascades[i].boundsMax.y) {
                shadow_index = i;
                break;
            }
        }
    }

    vec3 Lo = vec3(0.0);

    // directional light
    {
        float n_dot_l = dot(bsdf_params.geoN, uParams.sun.forward.xyz);

        float split = uShadowCascades[shadow_index].splitDistance;
        float blend_start = split * 0.5f;
        float blend_end   = split;
        float shadow_kernel_scale = 1.0f + 1.0f * saturate((bsdf_params.V.w - blend_start) / (blend_end - blend_start));
        shadow_kernel_scale *= 1.25;

        float shadow = 1.0f;
        if (shadow_index >= 0) {
            shadow = sampleShadowPoisson(uShadowCascades[shadow_index], uSunShadowMaps[shadow_index], in_shadow_position_ndc[shadow_index], n_dot_l, shadow_kernel_scale);
        }
        vec3 radiance = uParams.sun.radiance.xyz * shadow;
        if (any(greaterThan(radiance, LIGHT_EPSILON))) {
            Lo += bsdf(bsdf_params, uParams.sun.forward.xyz, radiance);
        }
    }

    // spot and point lights
    {
        uint light_tile_base_index = calculateTileLightBaseIndex(uvec2(gl_FragCoord.xy));
        uint light_tile_count = uTileLightIndices[light_tile_base_index + 0];

        for (int i = 0; i < light_tile_count; i++) {
            uint light_index = uTileLightIndices[light_tile_base_index + 1 + i];
            Lo += evaluateUberLight(uUberLightBuffer[light_index], bsdf_params);
        }
    }

    // ambient light
    {
        vec2 screen_uv = vec2(gl_FragCoord.xy) * uParams.viewport.zw;
        vec4 ao_raw = textureLod(uAmbientOcclusion, screen_uv, 0);
        vec3 bent_normal_vs = octahedronDecode(ao_raw.yz);// still unused
        float ao_combined = ao_raw.x * bsdf_params.occlusion;
        vec3 ao_multi_bounce = gtaoMultibounce(ao_combined, bsdf_params.albedo.rgb);

        float n_dot_v = max(dot(bsdf_params.N, bsdf_params.V.xyz), 0.0);
        vec3 kS = fresnelSchlickRoughness(n_dot_v, bsdf_params.f0, bsdf_params.roughness);
        vec3 kD = (1.0 - kS) * (1.0 - bsdf_params.metalness);

        vec3 irradiance = uParams.ambient.rgb;
        vec3 diffuse = irradiance * bsdf_params.albedo.rgb * ao_multi_bounce;

        // Where is ambient specular?
        vec3 ambient = diffuse * kD;
        Lo += ambient;
    }

    out_color = vec4(Lo, 1.0);

    if ((cParams.flags & FLAG_SHADOW_CASCADES) != 0x0) {
        out_color.rgb = mix(out_color.rgb, CASCADE_DEBUG_COLORS[shadow_index], 0.3f);
    }

    if ((cParams.flags & FLAG_LIGHT_DENSITY) != 0x0) {
        uint index = calculateTileLightBaseIndex(uvec2(gl_FragCoord.xy));
        uint count = uTileLightIndices[index];

        float density = 1.0 - float(count) / LIGHT_TILE_MAX_LIGHTS;
        // make small values a bit more apparent
        density *= density;
        density = 1.0 - density;
        if (density > 0.0) {
            vec4 jet_color = jet(density);
            out_color.rgb = mix(out_color.rgb, jet_color.rgb, 0.3f);
        }
    }
}