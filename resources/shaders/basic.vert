#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec4 in_tangent;
layout (location = 3) in vec3 in_texcoord;

layout (location = 0) out vec3 out_color;

layout (push_constant) uniform ExamplePushConstants
{
    mat4 transform;
} cExample;

void main ()
{
    gl_Position = cExample.transform * vec4(in_position, 1.0);
    out_color = vec3(1.0) * dot(in_normal, normalize((cExample.transform * vec4(1,1,1,1)).xyz));
}