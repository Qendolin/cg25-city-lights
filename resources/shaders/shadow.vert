#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;

#include "common/descriptors_geom.glsl"

layout (push_constant) uniform ShaderParamConstants
{
    mat4 projectionView;
    float extrusionBias;
    float pad0;
    float pad1;
    float pad2;
} cParams;

void main() {
    Section section = uSectionBuffer.sections[gl_DrawID];
    Instance instance = uInstanceBuffer.instances[section.instance];

    gl_Position = cParams.projectionView * instance.transform * vec4(in_position + in_normal * cParams.extrusionBias, 1.0);
}