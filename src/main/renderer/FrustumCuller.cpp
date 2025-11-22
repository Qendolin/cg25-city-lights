#include "FrustumCuller.h"

#include "../backend/Buffer.h"
#include "../backend/ShaderCompiler.h"
#include "../scene/Scene.h"
#include "../util/globals.h"
#include "../util/math.h"


FrustumCuller::~FrustumCuller() = default;

FrustumCuller::FrustumCuller(const vk::Device &device) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);
}

void FrustumCuller::execute(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const scene::GpuData &gpu_data,
        const glm::mat4 &view_projection_matrix,
        Buffer &output_draw_command_buffer,
        Buffer &output_draw_command_count_buffer,
        size_t count_write_index
) {
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mPipeline.pipeline);

    DescriptorSet descriptor_set = allocator.allocate(mShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            {descriptor_set.write(
                     ShaderParamsDescriptorLayout::InputDrawCommandBuffer,
                     vk::DescriptorBufferInfo{.buffer = *gpu_data.drawCommands, .offset = 0, .range = vk::WholeSize}
             ),
             descriptor_set.write(
                     ShaderParamsDescriptorLayout::OutputDrawCommandBuffer,
                     vk::DescriptorBufferInfo{.buffer = *output_draw_command_buffer, .offset = 0, .range = vk::WholeSize}
             ),
             descriptor_set.write(
                     ShaderParamsDescriptorLayout::DrawCommandCountBuffer,
                     vk::DescriptorBufferInfo{.buffer = *output_draw_command_count_buffer, .offset = count_write_index * sizeof(uint32_t) * 16, .range = vk::WholeSize}
             )},
            {}
    );
    cmd_buf.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, *mPipeline.layout, 0, {gpu_data.sceneDescriptor, descriptor_set}, {}
    );

    output_draw_command_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderWrite);
    output_draw_command_count_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderWrite);

    // World space frustum planes
    std::array<glm::vec4, 6> frustum_planes = util::extractFrustumPlanes(view_projection_matrix);
    ShaderParamsPushConstants push_constants = {.planes = frustum_planes};
    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);

    cmd_buf.dispatch(util::divCeil(gpu_data.drawCommandCount, 64u), 1u, 1u);
}
void FrustumCuller::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader) {
    auto comp_sh = shader_loader.loadFromSource(device, "resources/shaders/frustum_cull.comp");

    auto scene_descriptor_layout = scene::SceneDescriptorLayout(device);
    ComputePipelineConfig pipeline_config = {
        .descriptorSetLayouts = {scene_descriptor_layout, mShaderParamsDescriptorLayout},
        .pushConstants = {vk::PushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(ShaderParamsPushConstants)
        }}
    };

    mPipeline = createComputePipeline(device, pipeline_config, *comp_sh);
}
