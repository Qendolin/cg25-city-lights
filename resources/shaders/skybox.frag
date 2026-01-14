#version 460

layout(set = 0, binding = 0) uniform samplerCube skyboxDay;
layout(set = 0, binding = 1) uniform samplerCube skyboxNight;

layout(location = 0) in vec3 uvw;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
	mat4 projViewNoTranslation;
	vec4 tint;
	float blend;
	float pad0;
	float pad1;
	float pad2;
} push;

void main() {
	vec4 day = texture(skyboxDay, uvw);
	vec4 night = texture(skyboxNight, uvw);
	outColor = mix(day, night, push.blend) * push.tint;
}
