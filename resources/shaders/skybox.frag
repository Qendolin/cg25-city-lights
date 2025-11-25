#version 460

layout(set = 0, binding = 0) uniform samplerCube samplerCubeMap;

layout(location = 0) in vec3 uvw;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
	mat4 projViewNoTranslation;
	vec4 tint;
} push;

void main() {
	outColor = texture(samplerCubeMap, uvw) * push.tint;
}
