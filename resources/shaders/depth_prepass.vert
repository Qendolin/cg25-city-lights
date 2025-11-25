#version 460

#include "common/descriptors_geom.glsl"
#include "common/descriptors_mat.glsl"
#include "common/descriptors_light.glsl"

layout (location = 0) in vec3 in_position;

layout (push_constant) uniform ShaderPushConstants
{
    mat4 view;
    mat4 projection;
} cParams;

void main() {
    Section section = uSectionBuffer.sections[gl_InstanceIndex];
    Instance instance = uInstanceBuffer.instances[section.instance];

    vec4 position_ws = instance.transform * vec4(in_position, 1.0);
    gl_Position = cParams.projection * cParams.view * position_ws;
}