#include "FinalizeRenderer.h"

#include "../backend/Framebuffer.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../debug/Settings.h"
#include "../util/globals.h"
#include "../util/math.h"

FinalizeRenderer::~FinalizeRenderer() = default;

FinalizeRenderer::FinalizeRenderer(const vk::Device &device, const DescriptorAllocator &allocator) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);
    mShaderParamsDescriptors.create(util::MaxFramesInFlight, [&]() {
        return allocator.allocate(mShaderParamsDescriptorLayout);
    });
    mSampler = device.createSamplerUnique({});
}

void FinalizeRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader) {
    auto comp_sh = shader_loader.loadFromSource(device, "resources/shaders/finalize.comp");

    ComputePipelineConfig pipeline_config = {
        .descriptorSetLayouts = {mShaderParamsDescriptorLayout},
        .pushConstants = {vk::PushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(Settings::AgXParams)
        }}
    };

    mPipeline = createComputePipeline(device, pipeline_config, *comp_sh);
}

void FinalizeRenderer::execute(
        const vk::Device &device,
        const vk::CommandBuffer &cmd_buf,
        const Attachment &hdr_attachment,
        const Attachment &sdr_attachment,
        const Settings::AgXParams &agx_params
) {

    hdr_attachment.barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    sdr_attachment.barrier(cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);

    mShaderParamsDescriptors.next();
    device.updateDescriptorSets(
            {
                mShaderParamsDescriptors.get().write(
                        ShaderParamsDescriptorLayout::InColor,
                        vk::DescriptorImageInfo{
                            .sampler = *mSampler, .imageView = hdr_attachment.view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                mShaderParamsDescriptors.get().write(
                        ShaderParamsDescriptorLayout::OutColor,
                        vk::DescriptorImageInfo{.imageView = sdr_attachment.view, .imageLayout = vk::ImageLayout::eGeneral}
                ),
            },
            {}
    );
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mPipeline.layout, 0, {mShaderParamsDescriptors.get()}, {});
    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(agx_params), &agx_params);
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mPipeline.pipeline);
    cmd_buf.dispatch(util::divCeil(sdr_attachment.extents.width, 8u), util::divCeil(sdr_attachment.extents.height, 8u), 1);
}
