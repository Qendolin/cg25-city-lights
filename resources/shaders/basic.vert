#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_position;

layout (location = 0) out vec3 out_color;


layout (push_constant) uniform ExamplePushConstants
{
    mat4 transform;
} cExample;

void main ()
{
    gl_Position = cExample.transform * vec4(in_position, 1.0);
    out_color = normalize(in_position) * 0.5 + 0.5;
}