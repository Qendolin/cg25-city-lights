#version 460

// Temp Simple Shader

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 normalWorldSpace;

layout(push_constant) uniform Push {
	mat4 projViewModelMatrix;
	mat4 modelMatrix;
} push;

void main() {
	gl_Position = push.projViewModelMatrix * vec4(position, 1.0);

	normalWorldSpace = normalize(mat3(push.modelMatrix) * normal);
}