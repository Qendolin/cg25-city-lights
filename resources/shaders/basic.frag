#version 460

layout(location = 0) in vec3 in_color;

layout(location = 0) out vec4 out_color;

layout (std140, set = 0, binding = 0) uniform ExampleUniforms {
    float alpha;
} uExample;

void main() {
    out_color = vec4(in_color * uExample.alpha, 1.0);
}