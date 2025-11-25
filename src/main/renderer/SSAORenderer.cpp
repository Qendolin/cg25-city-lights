#include "SSAORenderer.h"

#include "../backend/DeviceQueue.h"
#include "../backend/Framebuffer.h"
#include "../backend/ImageResource.h"
#include "../backend/ShaderCompiler.h"
#include "../backend/StagingBuffer.h"
#include "../util/math.h"

SSAORenderer::~SSAORenderer() = default;

SSAORenderer::SSAORenderer(const vk::Device &device, const vma::Allocator &allocator, const DeviceQueue &graphicsQueue) {
    mSamplerShaderParamsDescriptorLayout = SamplerShaderParamsDescriptorLayout(device);
    mFilterShaderParamsDescriptorLayout = FilterShaderParamsDescriptorLayout(device);
    mDepthSampler = device.createSamplerUnique({
        .addressModeU = vk::SamplerAddressMode::eClampToBorder,
        .addressModeV = vk::SamplerAddressMode::eClampToBorder,
        .borderColor = vk::BorderColor::eFloatTransparentBlack,
    });

    auto cmd_pool = device.createCommandPoolUnique(
            {.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = graphicsQueue}
    );

    // FIXME: loading an image is way too cumbersome

    StagingBuffer staging = {allocator, device, *cmd_pool};

    PlainImageDataU8 noise_image_data =
            PlainImageDataU8::create(vk::Format::eR8G8Unorm, "resources/images/blue_noise_rotation.png");
    auto create_info = ImageCreateInfo::from(noise_image_data);
    create_info.usage |= vk::ImageUsageFlagBits::eSampled;

    mNoise = Image::create(staging.allocator(), create_info);
    util::setDebugName(device, *mNoise, "ssao_noise");

    auto staged_data = staging.stage(noise_image_data.pixels);
    mNoise.load(staging.commands(), 0, {}, staged_data);
    mNoise.barrier(staging.commands(), ImageResourceAccess::FragmentShaderReadOptimal);

    staging.submit(graphicsQueue);

    mNoiseView = mNoise.createDefaultView(device);
    util::setDebugName(device, *mNoiseView, "ssao_noise_view");
}

void SSAORenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const glm::mat4 &projection_mat,
        float z_near,
        const Attachment &depth_attachment,
        const Attachment &ao_raw,
        const Attachment &ao_filtered
) {
    util::ScopedCommandLabel dbg_cmd_label_func(cmd_buf);
    util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Sampling");

    depth_attachment.barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    ao_raw.barrier(cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);

    ShaderParamsInlineUniformBlock shader_params = {
        .projection = projection_mat,
        .zNear = z_near,
        .radius = radius,
        .bias = bias,
    };

    uint32_t ao_width = ao_raw.extents.width;
    uint32_t ao_height = ao_raw.extents.height;

    calculateInverseProjectionConstants(
            projection_mat, static_cast<float>(ao_width), static_cast<float>(ao_height),
            shader_params.inverseProjectionScale, shader_params.inverseProjectionOffset
    );

    auto descriptor_set_sampler = allocator.allocate(mSamplerShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            {
                descriptor_set_sampler.write(
                        SamplerShaderParamsDescriptorLayout::ShaderParams,
                        vk::WriteDescriptorSetInlineUniformBlock{
                            .dataSize = sizeof(shader_params),
                            .pData = &shader_params,
                        }
                ),
                descriptor_set_sampler.write(
                        SamplerShaderParamsDescriptorLayout::InDepth,
                        vk::DescriptorImageInfo{
                            .sampler = *mDepthSampler, .imageView = depth_attachment.view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set_sampler.write(
                        SamplerShaderParamsDescriptorLayout::OutRawAO,
                        vk::DescriptorImageInfo{.imageView = ao_raw.view, .imageLayout = vk::ImageLayout::eGeneral}
                ),
                descriptor_set_sampler.write(
                        SamplerShaderParamsDescriptorLayout::InNoise,
                        vk::DescriptorImageInfo{.imageView = *mNoiseView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal}
                ),
            },
            {}
    );
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mSamplerPipeline.layout, 0, {descriptor_set_sampler}, {});
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mSamplerPipeline.pipeline);
    cmd_buf.dispatch(util::divCeil(ao_width, 8u), util::divCeil(ao_height, 8u), 1);


    // Filtering

    dbg_cmd_label_region.swap("Filter");

    auto descriptor_set_filter = allocator.allocate(mFilterShaderParamsDescriptorLayout);
    ao_raw.barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    ao_filtered.barrier(cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);
    device.updateDescriptorSets(
            {
                descriptor_set_filter.write(
                        FilterShaderParamsDescriptorLayout::InRawAO,
                        vk::DescriptorImageInfo{
                            .sampler = *mDepthSampler, .imageView = ao_raw.view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set_filter.write(
                        FilterShaderParamsDescriptorLayout::InDepth,
                        vk::DescriptorImageInfo{
                            .sampler = *mDepthSampler, .imageView = depth_attachment.view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set_filter.write(
                        FilterShaderParamsDescriptorLayout::OutFilteredAO,
                        vk::DescriptorImageInfo{.imageView = ao_filtered.view, .imageLayout = vk::ImageLayout::eGeneral}
                ),
            },
            {}
    );

    FilterShaderPushConstants filter_params = {
        .zNear = z_near,
        .sharpness = filterSharpness,
        .exponent = exponent,
    };
    cmd_buf.pushConstants(*mFilterPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(filter_params), &filter_params);

    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mFilterPipeline.layout, 0, {descriptor_set_filter}, {});
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mFilterPipeline.pipeline);
    cmd_buf.dispatch(util::divCeil(ao_width, 8u), util::divCeil(ao_height, 8u), 1);
}

void SSAORenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader) {
    auto comp_sh_sampler = shader_loader.loadFromSource(device, "resources/shaders/ssao.comp");
    ComputePipelineConfig pipeline_config_sampler = {
        .descriptorSetLayouts = {mSamplerShaderParamsDescriptorLayout}, .pushConstants = {}
    };
    mSamplerPipeline = createComputePipeline(device, pipeline_config_sampler, *comp_sh_sampler);

    auto comp_sh_filter = shader_loader.loadFromSource(device, "resources/shaders/ssao_filter.comp");
    ComputePipelineConfig pipeline_config_filter = {
        .descriptorSetLayouts = {mFilterShaderParamsDescriptorLayout},
        .pushConstants = {vk::PushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(FilterShaderPushConstants)
        }}
    };
    mFilterPipeline = createComputePipeline(device, pipeline_config_filter, *comp_sh_filter);
}

void SSAORenderer::calculateInverseProjectionConstants(
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
