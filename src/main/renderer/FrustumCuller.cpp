#include "FrustumCuller.h"

#include "../backend/Buffer.h"
#include "../backend/ShaderCompiler.h"
#include "../debug/Annotation.h"
#include "../scene/Scene.h"
#include "../util/math.h"

FrustumCuller::~FrustumCuller() = default;

FrustumCuller::FrustumCuller(const vk::Device &device) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);
}

BufferRef FrustumCuller::execute(
        const vk::Device &device,
        const DescriptorAllocator &desc_alloc,
        const TransientBufferAllocator &buf_alloc,
        const vk::CommandBuffer &cmd_buf,
        const scene::GpuData &gpu_data,
        const glm::mat4 &view_projection_matrix
) const {
    util::ScopedCommandLabel dbg_cmd_label_func(cmd_buf);

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mPipeline.pipeline);

    size_t draw_command_buffer_size = gpu_data.drawCommandCount * sizeof(vk::DrawIndexedIndirectCommand);
    size_t draw_command_buffer_final_size = util::alignOffset(draw_command_buffer_size, 32) + 32;
    auto output_draw_command_buffer = buf_alloc.allocate(
            draw_command_buffer_final_size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
                                                    vk::BufferUsageFlagBits::eIndirectBuffer
    );
    util::setDebugName(device, output_draw_command_buffer.buffer, "culled_draw_commands");
    output_draw_command_buffer.barrier(
            cmd_buf,
            BufferResourceAccess{.stage = vk::PipelineStageFlagBits2::eDrawIndirect, .access = vk::AccessFlagBits2::eIndirectCommandRead},
            BufferResourceAccess::TransferWrite
    );
    // Count is placed at the end of the buffer
    cmd_buf.fillBuffer(output_draw_command_buffer, draw_command_buffer_final_size - 32, sizeof(uint32_t), 0);
    output_draw_command_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderStorageReadWrite);

    DescriptorSet descriptor_set = desc_alloc.allocate(mShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            {descriptor_set.write(
                     ShaderParamsDescriptorLayout::InputDrawCommandBuffer,
                     vk::DescriptorBufferInfo{.buffer = *gpu_data.drawCommands, .offset = 0, .range = vk::WholeSize}
             ),
             descriptor_set.write(
                     ShaderParamsDescriptorLayout::OutputDrawCommandBuffer,
                     vk::DescriptorBufferInfo{.buffer = output_draw_command_buffer, .offset = 0, .range = draw_command_buffer_size}
             ),
             descriptor_set.write(
                     ShaderParamsDescriptorLayout::DrawCommandCountBuffer,
                     vk::DescriptorBufferInfo{
                         .buffer = output_draw_command_buffer, .offset = draw_command_buffer_final_size - 32, .range = sizeof(uint32_t)
                     }
             )},
            {}
    );
    cmd_buf.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, *mPipeline.layout, 0, {gpu_data.sceneDescriptor, descriptor_set}, {}
    );

    // World space frustum planes
    std::array<glm::vec4, 6> frustum_planes = util::extractFrustumPlanes(view_projection_matrix);
    ShaderParamsPushConstants push_constants = {.planes = frustum_planes};
    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);

    cmd_buf.dispatch(util::divCeil(gpu_data.drawCommandCount, 64u), 1u, 1u);

    return output_draw_command_buffer;
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
