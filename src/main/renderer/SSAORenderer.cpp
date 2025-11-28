#include "SSAORenderer.h"

#include "../backend/DeviceQueue.h"
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
            PlainImageDataU8::create(vk::Format::eR8G8Unorm, "resources/images/gtao_blue_noise.png");
    ImageCreateInfo create_info = {
        .format = noise_image_data.format,
        .aspects = vk::ImageAspectFlagBits::eColor,
        .width = noise_image_data.width,
        .height = noise_image_data.height,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
    };
    mNoise = ImageWithView::create(device, staging.allocator(), create_info);
    util::setDebugName(device, *mNoise.image, "ssao_noise");
    util::setDebugName(device, *mNoise.view, "ssao_noise_view");

    auto staged_data = staging.stage(noise_image_data.pixels);
    mNoise.load(staging.commands(), 0, {}, staged_data);
    mNoise.barrier(staging.commands(), ImageResourceAccess::FragmentShaderReadOptimal);

    staging.submit(graphicsQueue);
}

void SSAORenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const glm::mat4 &projection_mat,
        float z_near,
        const ImageViewPairBase &depth_attachment,
        const ImageViewPairBase &ao_intermediary,
        const ImageViewPairBase &ao_result
) {
    util::ScopedCommandLabel dbg_cmd_label_func(cmd_buf);
    util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Sampling");

    depth_attachment.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    ao_result.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);

    ShaderParamsInlineUniformBlock shader_params = {
        .projection = projection_mat,
        .zNear = z_near,
        .radius = radius,
        .bias = bias,
    };

    uint32_t ao_width = ao_result.image().info.width;
    uint32_t ao_height = ao_result.image().info.height;

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
                            .sampler = *mDepthSampler, .imageView = depth_attachment.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set_sampler.write(
                        SamplerShaderParamsDescriptorLayout::OutRawAO,
                        vk::DescriptorImageInfo{.imageView = ao_result.view(), .imageLayout = vk::ImageLayout::eGeneral}
                ),
                descriptor_set_sampler.write(
                        SamplerShaderParamsDescriptorLayout::InNoise,
                        vk::DescriptorImageInfo{.imageView = mNoise, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal}
                ),
            },
            {}
    );
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mSamplerPipeline.layout, 0, {descriptor_set_sampler}, {});
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mSamplerPipeline.pipeline);
    cmd_buf.dispatch(util::divCeil(ao_width, 8u), util::divCeil(ao_height, 8u), 1);


    // Filtering
    FilterShaderPushConstants filter_params = {
        .zNear = z_near,
        .sharpness = filterSharpness,
        .exponent = 1.0f,
    };

    // First pass: Horizontal
    dbg_cmd_label_region.swap("Filter X");
    filter_params.direction = glm::vec2(1, 0);
    filterPass(device, allocator, cmd_buf, depth_attachment, ao_result, ao_intermediary, filter_params);

    // Second pass: Vertical
    dbg_cmd_label_region.swap("Filter Y");
    filter_params.direction = glm::vec2(0, 1);
    filter_params.exponent = exponent;
    filterPass(device, allocator, cmd_buf, depth_attachment, ao_intermediary, ao_result, filter_params);
}

void SSAORenderer::filterPass(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const ImageViewPairBase &depth_attachment,
        const ImageViewPairBase &ao_input,
        const ImageViewPairBase &ao_output,
        const FilterShaderPushConstants &filter_params
) {
    uint32_t ao_width = ao_input.image().info.width;
    uint32_t ao_height = ao_input.image().info.height;

    auto descriptor_set = allocator.allocate(mFilterShaderParamsDescriptorLayout);
    ao_input.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    ao_output.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);
    device.updateDescriptorSets(
            {
                descriptor_set.write(
                        FilterShaderParamsDescriptorLayout::InRawAO,
                        vk::DescriptorImageInfo{
                            .sampler = *mDepthSampler, .imageView = ao_input.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set.write(
                        FilterShaderParamsDescriptorLayout::InDepth,
                        vk::DescriptorImageInfo{
                            .sampler = *mDepthSampler, .imageView = depth_attachment.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                descriptor_set.write(
                        FilterShaderParamsDescriptorLayout::OutFilteredAO,
                        vk::DescriptorImageInfo{.imageView = ao_output.view(), .imageLayout = vk::ImageLayout::eGeneral}
                ),
            },
            {}
    );

    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mFilterPipeline.layout, 0, {descriptor_set}, {});
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mFilterPipeline.pipeline);

    cmd_buf.pushConstants(*mFilterPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(filter_params), &filter_params);
    cmd_buf.dispatch(util::divCeil(ao_width, 16u), util::divCeil(ao_height, 16u), 1);
}


void SSAORenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, int slices, int samples) {
    auto comp_sh_sampler = shader_loader.loadFromSource(device, "resources/shaders/ssao.comp");
    ComputePipelineConfig pipeline_config_sampler = {
        .descriptorSetLayouts = {mSamplerShaderParamsDescriptorLayout}, .pushConstants = {}
    };
    auto specialization_constants = SpecializationConstantsBuilder().add(0, slices).add(1, samples).build();
    mSamplerPipeline = createComputePipeline(device, pipeline_config_sampler, *comp_sh_sampler, specialization_constants);

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
