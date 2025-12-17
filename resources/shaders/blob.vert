#version 460

// Temp Simple Shader

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

layout(location = 0) out vec3 out_position_ws;
layout(location = 1) out vec3 out_normal;

layout(push_constant) uniform Push {
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 modelMatrix;
	vec4 camera;
	vec2 invViewportSize;
} push;

void main() {
	vec4 position_ws = push.modelMatrix * vec4(in_position, 1.0);
	out_position_ws = position_ws.xyz;
	gl_Position = push.projectionMatrix * push.viewMatrix * position_ws;

	out_normal = normalize(mat3(push.modelMatrix) * in_normal);
}