#version 460

// Temp Simple Shader

layout (location = 0) in vec3 normalWorldSpace;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
	mat4 projViewModelMatrix;
	mat4 modelMatrix;
} push;

const vec3 DIR_TO_LIGHT = normalize(vec3(0.0, 1.0, 1.0));
const vec3 GREEN = vec3(0., 1., 0.);
const float AMBIENT = 0.02;

void main() {

	float lightIntensity = AMBIENT + max(0.0, dot(normalWorldSpace, DIR_TO_LIGHT));

	outColor = vec4(lightIntensity * GREEN, 1.0);
}