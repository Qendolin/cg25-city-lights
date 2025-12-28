#version 460

// Temp Simple Shader

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_tangent;

layout(location = 0) out vec3 out_position_ws;
layout(location = 1) out vec3 out_position_ls;
layout(location = 2) out mat3 out_tbn;

layout(std140, set = 0, binding = 1) uniform ShaderParams {
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 modelMatrix;
	vec4 camera;
	vec2 invViewportSize;
} uParams;

void main() {
	out_position_ls = in_position;
	vec4 position_ws = uParams.modelMatrix * vec4(in_position, 1.0);
	out_position_ws = position_ws.xyz;
	gl_Position = uParams.projectionMatrix * uParams.viewMatrix * position_ws;

	mat3 normal_matrix = mat3(uParams.modelMatrix);
	vec3 T = normalize(normal_matrix * in_tangent.xyz);
	vec3 N = normalize(normal_matrix * in_normal);
	vec3 bitangent = cross(in_normal, in_tangent.xyz) * in_tangent.w;
	vec3 B = normalize(normal_matrix * bitangent);
	out_tbn = mat3(T, B, N);
}