#include "FogLightRenderer.h"

#include "../backend/Buffer.h"
#include "../backend/Framebuffer.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../debug/Annotation.h"
#include "../scene/Scene.h"
#include "../util/math.h"


struct UnmanagedBuffer;
namespace scene {
    struct GpuData;
}
FogLightRenderer::~FogLightRenderer() = default;

FogLightRenderer::FogLightRenderer(const vk::Device &device) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);
}

void FogLightRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader) {
    auto comp_sh = shader_loader.loadFromSource(device, "resources/shaders/light_froxel_assign.comp");

    ComputePipelineConfig pipeline_config = {
        .descriptorSetLayouts = {mShaderParamsDescriptorLayout},
        .pushConstants = {vk::PushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(ShaderPushConstants)
        }}
    };

    mPipeline = createComputePipeline(device, pipeline_config, *comp_sh);
    util::setDebugName(device, *mPipeline.pipeline, "light_froxel_assign");
}

void FogLightRenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const vk::Buffer &light_buffer,
        const glm::mat4 &projection_mat,
        const glm::mat4 &view_mat,
        float z_near,
        const BufferBase &cluster_light_indices_buffer
) {
    cluster_light_indices_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderWrite);

    auto descriptor_set = allocator.allocate(mShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            {
                descriptor_set.write(
                        ShaderParamsDescriptorLayout::TileLightIndices,
                        {.buffer = cluster_light_indices_buffer, .offset = 0, .range = vk::WholeSize}
                ),
                descriptor_set.write(
                       ShaderParamsDescriptorLayout::UberLights,
                       {.buffer = light_buffer, .offset = 0, .range = vk::WholeSize}
               ),
            },
            {}
    );
    cmd_buf.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, *mPipeline.layout, 0, {descriptor_set}, {}
    );

    glm::mat4 inverse_view = glm::inverse(view_mat);
    ShaderPushConstants push_constants = {
        .inverseViewMatrix = inverse_view,
        .cameraPosition = inverse_view[3],
        .zNear = z_near,
        .cameraForward = -inverse_view[2],
    };

    calculateInverseProjectionConstants(
            projection_mat, CLUSTER_DIM_X, CLUSTER_DIM_Y,
            push_constants.inverseProjectionScale, push_constants.inverseProjectionOffset
    );

    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mPipeline.pipeline);
    cmd_buf.dispatch(CLUSTER_DIM_X, CLUSTER_DIM_Y, 1);
}


void FogLightRenderer::calculateInverseProjectionConstants(
        const glm::mat4 &projectionMatrix, float textureWidth, float textureHeight, glm::vec2 &viewScale, glm::vec2 &viewOffset
) {
    float P_inv_00 = 1.0f / projectionMatrix[0][0];
    float P_inv_11 = 1.0f / projectionMatrix[1][1];

    // Fast inverse projection is just `p * A + B` where p are the screen space coordinates

    // A = (2.0 / ScreenSize) * P_inv
    viewScale.x = 2.0f * P_inv_00 / textureWidth;
    viewScale.y = 2.0f * P_inv_11 / textureHeight;

    // B = (-1.0 + 1.0/ScreenSize) * P_inv
    viewOffset.x = P_inv_00 * (1.0f / textureWidth - 1.0f);
    viewOffset.y = P_inv_11 * (1.0f / textureHeight - 1.0f);
}
