#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](vec2 (0.0, -0.5), vec2 (0.5, 0.5), vec2 (-0.5, 0.5));

vec3 colors[3] = vec3[](vec3 (1.0, 0.0, 0.0), vec3 (0.0, 1.0, 0.0), vec3 (0.0, 0.0, 1.0));

layout (push_constant) uniform ExamplePushConstants
{
    float angle;
} cExample;

void main ()
{
    vec2 pos = positions[gl_VertexIndex];
    mat2 rot = mat2(
        cos(cExample.angle), -sin(cExample.angle), // 1st column
        sin(cExample.angle), cos(cExample.angle)); // 2nd column
    gl_Position = vec4 (rot * pos, 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}