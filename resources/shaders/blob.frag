#version 460

#include "pbr/fresnel.glsl"

layout (location = 0) in vec3 in_position_ws;
layout (location = 1) in vec3 in_normal;

layout (location = 0) out vec4 out_color;

layout(push_constant) uniform Push {
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 modelMatrix;
	vec4 camera;
	vec2 invViewportSize;
} push;

layout (set = 0, binding = 0) uniform sampler2D uStoredHdrColorImage;

const vec3 DIR_TO_LIGHT = normalize(vec3(0.0, 1.0, 1.0));
const vec3 ALBEDO = vec3(0., 0.8, 0.);

void main() {
	vec3 N = normalize(in_normal);
	vec3 L = DIR_TO_LIGHT;
	vec3 V = -normalize(in_position_ws - push.camera.xyz);
	vec3 H = normalize(V + L);

	// DISClAIMER
	// This code is not good. It's just a prototype

	float n_dot_h = max(dot(N, H), 0.0);
	float specular = pow(n_dot_h, 100.0);

	vec2 sample_uv = gl_FragCoord.xy * push.invViewportSize;

	// Refraction (approximate, ad hoc)
	{
		sample_uv = sample_uv * 2.0 - 1.0;

		vec3 refracted_ws = refract(V, N, 1.0/1.5);
		vec3 refracted = transpose(mat3(push.modelMatrix)) * refracted_ws;
		sample_uv += refracted_ws.xy * 0.2;

		sample_uv = sample_uv * 0.5 + 0.5;
	}

	vec3 transmission = vec3(0.0);

	// dirty blur
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(0, 0)).rgb;
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(+1, +1)).rgb;
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(+1, -1)).rgb;
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(-1, +1)).rgb;
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(-1, -1)).rgb;
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(+2, 0)).rgb;
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(-2, 0)).rgb;
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(0, +2)).rgb;
	transmission += textureLodOffset(uStoredHdrColorImage, sample_uv, 0, ivec2(0, -2)).rgb;

	transmission /= 9.0;

	vec3 color = vec3(0.0);

	// Direct
	{
		color += pow(1.0 - max(dot(N, V), 0.0) * 0.7, 3.0) * transmission * ALBEDO + vec3(specular);
	}

	// Ambient
	{
		float n_dot_v = max(dot(N, V), 0.0);
		vec3 kS = fresnelSchlick(n_dot_v, vec3(0.04));
		color += vec3(0.3) * kS;
	}

	out_color = vec4(color, 1.0);
}