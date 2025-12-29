#include "LightRenderer.h"

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
LightRenderer::~LightRenderer() = default;

LightRenderer::LightRenderer(const vk::Device &device) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);
    mDepthSampler = device.createSamplerUnique({
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
    });
}

void LightRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader) {
    auto comp_sh = shader_loader.loadFromSource(device, "resources/shaders/light_tile_assign.comp");

    scene::SceneDescriptorLayout scene_descriptor_layout(device);
    ComputePipelineConfig pipeline_config = {
        .descriptorSetLayouts = {scene_descriptor_layout, mShaderParamsDescriptorLayout},
        .pushConstants = {vk::PushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eCompute, .offset = 0, .size = sizeof(ShaderPushConstants)
        }}
    };

    mPipeline = createComputePipeline(device, pipeline_config, *comp_sh);
    util::setDebugName(device, *mPipeline.pipeline, "light_tile_assign");
}

void LightRenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &allocator,
        const vk::CommandBuffer &cmd_buf,
        const scene::GpuData &gpu_data,
        const glm::mat4 &projection_mat,
        const glm::mat4 &view_mat,
        float z_near,
        const ImageViewPairBase &depth_attachment,
        const BufferBase &tile_light_indices_buffer
) {
    depth_attachment.image().barrier(cmd_buf, ImageResourceAccess::ComputeShaderReadOptimal);
    tile_light_indices_buffer.barrier(cmd_buf, BufferResourceAccess::ComputeShaderWrite);

    auto descriptor_set = allocator.allocate(mShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            {
                descriptor_set.write(
                        ShaderParamsDescriptorLayout::InDepth, {.sampler = *mDepthSampler,
                                                                .imageView = depth_attachment.view(),
                                                                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal}
                ),
                descriptor_set.write(
                        ShaderParamsDescriptorLayout::TileLightIndices,
                        {.buffer = tile_light_indices_buffer, .offset = 0, .range = vk::WholeSize}
                ),
            },
            {}
    );
    cmd_buf.bindDescriptorSets(
            vk::PipelineBindPoint::eCompute, *mPipeline.layout, 0, {gpu_data.sceneDescriptor, descriptor_set}, {}
    );

    ShaderPushConstants push_constants = {
        .inverseViewMatrix = glm::inverse(view_mat),
        .zNear = z_near,
        .lightRangeFactor = lightRangeFactor,
    };

    uint32_t out_width = depth_attachment.image().info.width;
    uint32_t out_height = depth_attachment.image().info.height;

    calculateInverseProjectionConstants(
            projection_mat, static_cast<float>(out_width), static_cast<float>(out_height),
            push_constants.inverseProjectionScale, push_constants.inverseProjectionOffset
    );

    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(push_constants), &push_constants);
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eCompute, *mPipeline.pipeline);
    cmd_buf.dispatch(util::divCeil(out_width, 16u), util::divCeil(out_height, 16u), 1);
}


void LightRenderer::calculateInverseProjectionConstants(
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
