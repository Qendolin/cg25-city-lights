#version 460

#include "pbr/fresnel.glsl"
#include "blob_noise.glsl"

layout (location = 0) in vec3 in_position_ws;
layout (location = 1) in vec3 in_position_ls;
layout (location = 2) in mat3 in_tbn;

layout (location = 0) out vec4 out_color;

layout (set = 0, binding = 0) uniform sampler2D uStoredHdrColorImage;

layout(std140, set = 0, binding = 1) uniform ShaderParams {
	mat4 projectionViewMatrix;
	mat4 modelMatrix;
	vec4 camera;
	vec2 invViewportSize;
	float pad0;
	float pad1;
	vec4 sunDir;
	vec4 sunLight;
	vec4 ambientLight;
} uParams;

float henyeyGreenstein(float g, float cos_theta) {
	float g2 = g * g;
	float num = 1.0 - g2;
	float denom = 4.0 * PI * pow(1.0 + g2 - 2.0 * g * cos_theta, 1.5);
	return num / max(denom, 0.0001);
}

const vec3 ALBEDO = vec3(0., 0.8, 0.);
const float SUN_G = 0.8;

void main() {

	vec3 V = -normalize(in_position_ws - uParams.camera.xyz);

	int noise_style = 0;
	float noise_scale = 4.0;
	float noise_time = 0.0;
	float noise_strength = 0.1;
	vec3 tN = perturbNormal(in_position_ls * noise_scale, noise_time, noise_style, noise_strength);
	tN *= step(-dot(V, in_tbn[2].xyz), 0.0) * 2.0 - 1.0; // flip shading normal when inside

	vec3 N = normalize(in_tbn * tN);
	vec3 L = uParams.sunDir.xyz;
	vec3 H = normalize(V + L);

	// DISClAIMER
	// This code is not good. It's just a prototype

	float n_dot_h = max(dot(N, H), 0.0);
	float n_dot_v = max(dot(N, V), 0.0);
	float specular = pow(n_dot_h, 100.0);

	vec2 sample_uv = gl_FragCoord.xy * uParams.invViewportSize;

	// Refraction (approximate, ad hoc)
	{
		sample_uv = sample_uv * 2.0 - 1.0;

		vec3 refracted_ws = refract(V, N, 1.0/1.5);
		vec3 refracted = transpose(mat3(uParams.modelMatrix)) * refracted_ws;
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


	// compute HG phase factor for scattering of sun into view
	// scattering angle is between incoming light direction (-L) and view direction V
	float cosTheta = dot(V, -L);
	float phase = henyeyGreenstein(SUN_G, cosTheta);

	// Direct
	{
		// original direct lighting (transmission + specular)
		color += pow(1.0 - n_dot_v * 0.5, 3.0) * transmission * ALBEDO * 2.0f;
		color += specular * uParams.sunLight.rgb;

		// Add HG single-scattering contribution from the sun (scattered into the view)
		// Multiply by transmission*ALBEDO so the medium / object affects amount scattered into view.
		color += ALBEDO * uParams.sunLight.rgb * phase * 0.1f;
	}

	// Ambient
	{
		vec3 kS = fresnelSchlick(n_dot_v, vec3(0.04));
		color += uParams.ambientLight.rgb * kS * ALBEDO;
	}

	out_color = vec4(color, 1.0);
}