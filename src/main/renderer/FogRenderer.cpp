#include "FogRenderer.h"

#include "../backend/Buffer.h"
#include "../backend/Framebuffer.h"
#include "../backend/Image.h"
#include "../backend/Pipeline.h"
#include "../backend/ShaderCompiler.h"
#include "../debug/Annotation.h"
#include "../debug/Settings.h"
#include "../entity/ShadowCaster.h"
#include "../util/math.h"

FogRenderer::~FogRenderer() = default;

FogRenderer::FogRenderer(const vk::Device &device) {
    mSampleShaderParamsDescriptorLayout = SampleShaderParamsDescriptorLayout(device);
    mFilterShaderParamsDescriptorLayout = FilterShaderParamsDescriptorLayout(device);
    mDepthSampler = device.createSamplerUnique({
        .addressModeU = vk::SamplerAddressMode::eClampToBorder,
        .addressModeV = vk::SamplerAddressMode::eClampToBorder,
        .borderColor = vk::BorderColor::eFloatTransparentBlack,
    });
    mShadowSampler = device.createSamplerUnique({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .compareEnable = true,
        .compareOp = vk::CompareOp::eGreaterOrEqual,
    });
    mResultSampler = device.createSamplerUnique({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
    });
}

void FogRenderer::recreate(
        const vk::Device &device, const ShaderLoader &shader_loader, const vma::Allocator &alloc, vk::Extent2D result_extent
) {
    createPipeline(device, shader_loader);

    mResultImage = std::make_unique<ImageWithView>(std::move(ImageWithView::create(
            device, alloc,
            {
                .format = vk::Format::eR16G16B16A16Sfloat,
                .aspects = vk::ImageAspectFlagBits::eColor,
                .width = result_extent.width,
                .height = result_extent.height,
                .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled,
                .device = vma::MemoryUsage::eGpuOnly,
            }
    )));
    util::setDebugName(device, *mResultImage->image, "fog_result_image");
    util::setDebugName(device, *mResultImage->view, "fog_result_image_view");
}

void FogRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader) {
    {
        auto comp_sh = shader_loader.loadFromSource(device, "resources/shaders/fog.comp");

        ComputePipelineConfig pipeline_config = {
            .descriptorSetLayouts = {mSampleShaderParamsDescriptorLayout},
            .pushConstants = {vk::PushConstantRange{
                .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(SamplePushConstants)
            }}
        };

        mSamplePipeline = createComputePipeline(device, pipeline_config, *comp_sh);
        util::setDebugName(device, *mSamplePipeline.pipeline, "fog_sample");
    }

    {
        auto comp_sh = shader_loader.loadFromSource(device, "resources/shaders/fog_filter.comp");

        ComputePipelineConfig pipeline_config = {
            .descriptorSetLayouts = {mFilterShaderParamsDescriptorLayout},
            .pushConstants = {vk::PushConstantRange{
                .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(FilterPushConstants)
            }}
        };

        mFilterPipeline = createComputePipeline(device, pipeline_config, *comp_sh);
        util::setDebugName(device, *mFilterPipeline.pipeline, "fog_filter");
    }
}

void FogRenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &descriptor_allocator,
        const TransientBufferAllocator &buffer_allocator,
        const vk::CommandBuffer &cmd_buf,
        const ImageViewPairBase &depth_attachment,
        const ImageViewPairBase &hdr_result_image,
        const DirectionalLight &sun_light,
        const glm::vec3 &ambient_light,
        const glm::vec3 &fog_color,
        std::span<const CascadedShadowCaster> sun_shadow_cascades,
        const glm::mat4 &view_mat,
        const glm::mat4 &projection_mat,
        float z_near,
        uint32_t frame_nr,
        const vk::Buffer &light_buffer,
        const BufferBase &cluster_buffer
) {

    if (density == 0.0)
        return;

    util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Setup");

    depth_attachment.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    mResultImage->barrier(cmd_buf, ImageResourceAccess::ComputeShaderWriteGeneral);
    // light_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderRead);
    cluster_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderRead);

    glm::mat4 inverse_view = glm::inverse(view_mat);
    glm::vec3 camera_pos_ws = inverse_view[3];

    glm::mat3 sun_rotation = sun_light.rotation();
    glm::vec3 camera_pos_ls = glm::transpose(sun_rotation) * camera_pos_ws;
    std::array<ShadowCascadeUniformBlock, Settings::SHADOW_CASCADE_COUNT> shadow_cascade_uniform_blocks = {};
    for (size_t i = 0; i < sun_shadow_cascades.size(); i++) {
        const auto &cascade = sun_shadow_cascades[i];

        glm::vec3 center_ws = -glm::transpose(glm::mat3(cascade.viewMatrix)) * glm::vec3(cascade.viewMatrix[3]);
        glm::vec3 center_ls = glm::transpose(sun_rotation) * center_ws;
        float extent_x = 1.0f / cascade.projectionMatrix[0][0];
        float extent_y = 1.0f / cascade.projectionMatrix[1][1];

        glm::vec2 relative_center_ls = glm::vec2(center_ls.x - camera_pos_ls.x, center_ls.y - camera_pos_ls.y);

        shadow_cascade_uniform_blocks[i] = {
            .transform = cascade.projectionMatrix * cascade.viewMatrix * inverse_view,
            .boundsMin = glm::vec2(relative_center_ls.x - extent_x, relative_center_ls.y - extent_y),
            .boundsMax = glm::vec2(relative_center_ls.x + extent_x, relative_center_ls.y + extent_y),
        };
    }

    UnmanagedBuffer shadow_cascade_uniform_buffer = buffer_allocator.allocate(
            sun_shadow_cascades.size_bytes(), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst
    );
    util::setDebugName(device, shadow_cascade_uniform_buffer.buffer, "shadow_cascades_uniform_buffer");
    shadow_cascade_uniform_buffer.barrier(cmd_buf, BufferResourceAccess::TransferWrite);
    cmd_buf.updateBuffer(
            shadow_cascade_uniform_buffer.buffer, 0, sizeof(shadow_cascade_uniform_blocks),
            shadow_cascade_uniform_blocks.data()
    );
    shadow_cascade_uniform_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderUniformRead);

    auto sample_descriptor_set = descriptor_allocator.allocate(mSampleShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            {
                sample_descriptor_set.write(
                        SampleShaderParamsDescriptorLayout::InDepth,
                        vk::DescriptorImageInfo{
                            .sampler = *mDepthSampler, .imageView = depth_attachment.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                sample_descriptor_set.write(
                        SampleShaderParamsDescriptorLayout::OutColor,
                        vk::DescriptorImageInfo{.imageView = *mResultImage, .imageLayout = vk::ImageLayout::eGeneral}
                ),
                sample_descriptor_set.write(
                        SampleShaderParamsDescriptorLayout::ShadowCascadeUniforms,
                        {.buffer = shadow_cascade_uniform_buffer, .offset = 0, .range = vk::WholeSize}
                ),
                sample_descriptor_set.write(
                        SampleShaderParamsDescriptorLayout::UberLights,
                        {.buffer = light_buffer, .offset = 0, .range = vk::WholeSize}
                ),
                sample_descriptor_set.write(
                        SampleShaderParamsDescriptorLayout::ClusterLightIndices,
                        {.buffer = cluster_buffer, .offset = 0, .range = vk::WholeSize}
                ),
            },
            {}
    );

    for (uint32_t i = 0; i < sun_shadow_cascades.size(); i++) {
        sun_shadow_cascades[i].framebuffer().depthAttachment.image().barrier(
                cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal
        );
        device.updateDescriptorSets(
                sample_descriptor_set.write(
                        SampleShaderParamsDescriptorLayout::SunShadowMap,
                        vk::DescriptorImageInfo{
                            .sampler = *mShadowSampler,
                            .imageView = sun_shadow_cascades[i].framebuffer().depthAttachment.view(),
                            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        },
                        i
                ),
                {}
        );
    }

    glm::vec3 world_up_vs = glm::vec3(view_mat * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));

    SamplePushConstants sample_push_consts = {
        .inverseViewMatrix = inverse_view,
        .inverseProjectionScale = {},
        .inverseProjectionOffset = {},
        .sunUpVS = glm::normalize(glm::mat3(view_mat) * sun_rotation[1]),
        .zNear = z_near,
        .sunRightVS = glm::normalize(glm::mat3(view_mat) * sun_rotation[0]),
        .density = density,
        .sunRadiance = sun_light.radiance(),
        .targetStepContribution = targetStepContribution,
        .ambientRadiance = ambient_light,
        .cameraHeight = camera_pos_ws.y,
        .worldUpVS = world_up_vs,
        .heightFalloff = heightFalloff,
        .fogColor = fog_color,
        .samples = samples,
        .sunDirVS = glm::normalize(glm::mat3(view_mat) * sun_rotation[2]),
        .g = g,
        .cameraPosition = camera_pos_ws,
        .frame = frame_nr,
    };

    auto width = mResultImage->imageInfo().width;
    auto height = mResultImage->imageInfo().height;

    calculateInverseProjectionConstants(
            projection_mat, static_cast<float>(width), static_cast<float>(height),
            sample_push_consts.inverseProjectionScale, sample_push_consts.inverseProjectionOffset
    );

    dbg_cmd_label_region.swap("Draw");

    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mSamplePipeline.layout, 0, {sample_descriptor_set}, {});
    cmd_buf.pushConstants(
            *mSamplePipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(sample_push_consts), &sample_push_consts
    );
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mSamplePipeline.pipeline);
    cmd_buf.dispatch(util::divCeil(width, 8u), util::divCeil(height, 8u), 1);

    dbg_cmd_label_region.swap("Filter");

    mResultImage->barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    hdr_result_image.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadWriteGeneral);

    auto filter_descriptor_set = descriptor_allocator.allocate(mFilterShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            {
                filter_descriptor_set.write(
                        FilterShaderParamsDescriptorLayout::InDepth,
                        vk::DescriptorImageInfo{
                            .sampler = *mDepthSampler, .imageView = depth_attachment.view(), .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
                        }
                ),
                filter_descriptor_set.write(
                        FilterShaderParamsDescriptorLayout::InSource,
                        vk::DescriptorImageInfo{.sampler = *mResultSampler, .imageView = *mResultImage, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal}
                ),
                filter_descriptor_set.write(
                        FilterShaderParamsDescriptorLayout::OutColor,
                        vk::DescriptorImageInfo{.imageView = hdr_result_image, .imageLayout = vk::ImageLayout::eGeneral}
                ),
            },
            {}
    );
    FilterPushConstants filter_push_constants = {
        .zNear = z_near,
        .sharpness = 10.0f,
    };

    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *mFilterPipeline.layout, 0, {filter_descriptor_set}, {});
    cmd_buf.pushConstants(
            *mFilterPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(filter_push_constants), &filter_push_constants
    );
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mFilterPipeline.pipeline);
    cmd_buf.dispatch(
            util::divCeil(hdr_result_image.image().info.width, 8u),
            util::divCeil(hdr_result_image.image().info.height, 8u), 1
    );
}


void FogRenderer::calculateInverseProjectionConstants(
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
