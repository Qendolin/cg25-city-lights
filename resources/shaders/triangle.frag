#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 fragColor;

layout (location = 0) out vec4 outColor;

layout (std140, set = 0, binding = 0) uniform ExampleUniforms {
    float alpha;
} uExample;

void main () { outColor = vec4 (fragColor * vec3(uExample.alpha), 1.0); }