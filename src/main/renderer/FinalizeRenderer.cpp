#include "FinalizeRenderer.h"

#include "../backend/Framebuffer.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../debug/Annotation.h"
#include "../debug/Settings.h"
#include "../util/math.h"

FinalizeRenderer::~FinalizeRenderer() = default;

FinalizeRenderer::FinalizeRenderer(const vk::Device &device) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);
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
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const ImageViewPairBase &hdr_attachment,
        const ImageViewPairBase &sdr_attachment,
        const Settings::AgXParams &agx_params
) {
    util::ScopedCommandLabel dbg_cmd_label_func(cmd_buf);

    hdr_attachment.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    sdr_attachment.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);

    auto descriptor_set = allocator.allocate(mShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            {
                descriptor_set.write(
                        ShaderParamsDescriptorLayout::InColor,
                        vk::DescriptorImageInfo{
                            .sampler = *mSampler, .imageView = hdr_attachment.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set.write(
                        ShaderParamsDescriptorLayout::OutColor,
                        vk::DescriptorImageInfo{.imageView = sdr_attachment.view(), .imageLayout = vk::ImageLayout::eGeneral}
                ),
            },
            {}
    );
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mPipeline.layout, 0, {descriptor_set}, {});
    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(agx_params), &agx_params);
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mPipeline.pipeline);
    cmd_buf.dispatch(util::divCeil(sdr_attachment.image().info.width, 8u), util::divCeil(sdr_attachment.image().info.height, 8u), 1);
}
