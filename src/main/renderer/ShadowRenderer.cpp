#include "ShadowRenderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../backend/ShaderCompiler.h"
#include "../debug/Annotation.h"
#include "../entity/ShadowCaster.h"
#include "../scene/Scene.h"


ShadowRenderer::~ShadowRenderer() = default;
ShadowRenderer::ShadowRenderer() = default;

void ShadowRenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &desc_alloc,
        const TransientBufferAllocator &buf_alloc,
        const vk::CommandBuffer &cmd_buf,
        const scene::GpuData &gpu_data,
        const FrustumCuller &frustum_culler,
        const ShadowCaster &shadow_caster,
        const ShadowCaster *inner_shadow_caster
) {
    // Culling
    util::ScopedCommandLabel dbg_cmd_label_region(cmd_buf, "Culling");

    glm::mat4 frustum_matrix = shadow_caster.projectionMatrix * shadow_caster.viewMatrix;
    glm::mat4 *exclude_matrix = nullptr;
    if (inner_shadow_caster) {
        glm::mat4 inner_frustum_matrix = inner_shadow_caster->projectionMatrix * inner_shadow_caster->viewMatrix;
        exclude_matrix = &inner_frustum_matrix;
    }

    // Cull objects smaller than ~1 texel
    float scale_x = shadow_caster.projectionMatrix[0][0];
    float scale_y = shadow_caster.projectionMatrix[1][1];
    float half_extent = std::max(1.0f / scale_x, 1.0f / scale_y);
    float min_world_radius = half_extent / shadow_caster.resolution();

    UnmanagedBuffer culled_commands = frustum_culler.execute(
            device, desc_alloc, buf_alloc, cmd_buf, gpu_data, frustum_matrix, exclude_matrix, min_world_radius
    );
    culled_commands.barrier(cmd_buf, BufferResourceAccess::IndirectCommandRead);

    // Rendering
    dbg_cmd_label_region.swap("Rendering");

    shadow_caster.framebuffer().depthAttachment.image().barrier(
            cmd_buf, ImageResourceAccess::DepthAttachmentEarlyOps, ImageResourceAccess::DepthAttachmentLateOps
    );

    const Framebuffer &fb = shadow_caster.framebuffer();
    cmd_buf.beginRendering(fb.renderingInfo({
        .enableColorAttachments = false,
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eDontCare},
        .colorStoreOps = {vk::AttachmentStoreOp::eDontCare},
        .depthLoadOp = vk::AttachmentLoadOp::eClear,
    }));

    mPipeline.config.viewports = {{fb.viewport(false)}};
    mPipeline.config.scissors = {{fb.area()}};
    mPipeline.config.depth.biasConstant = shadow_caster.depthBiasConstant;
    mPipeline.config.depth.biasClamp = shadow_caster.depthBiasClamp;
    mPipeline.config.depth.biasSlope = shadow_caster.depthBiasSlope;
    mPipeline.config.apply(cmd_buf);

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline.pipeline);
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mPipeline.layout, 0, {gpu_data.sceneDescriptor}, {});
    cmd_buf.bindIndexBuffer(*gpu_data.indices, 0, vk::IndexType::eUint32);
    cmd_buf.bindVertexBuffers(0, {*gpu_data.positions, *gpu_data.normals}, {0, 0});

    ShaderParamsPushConstants shader_params = {
        .projectionViewMatrix = frustum_matrix,
        .sizeBias = shadow_caster.extrusionBias / static_cast<float>(shadow_caster.resolution()),
    };
    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(shader_params), &shader_params);

    cmd_buf.drawIndexedIndirectCount(
            culled_commands, 0, culled_commands, culled_commands.size - 32, gpu_data.drawCommandCount,
            sizeof(vk::DrawIndexedIndirectCommand)
    );

    cmd_buf.endRendering();
}

void ShadowRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader) {
    auto vert_sh = shader_loader.loadFromSource(device, "resources/shaders/shadow.vert");

    auto scene_descriptor_layout = scene::SceneDescriptorLayout(device);
    GraphicsPipelineConfig pipeline_config = {
        .vertexInput =
                {
                    .bindings =
                            {
                                {.binding = 0, .stride = sizeof(glm::vec3), .inputRate = vk::VertexInputRate::eVertex}, // position
                                {.binding = 1, .stride = sizeof(glm::vec3), .inputRate = vk::VertexInputRate::eVertex}, // normal
                            },
                    .attributes =
                            {
                                {.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = 0}, // position
                                {.location = 1, .binding = 1, .format = vk::Format::eR32G32B32Sfloat, .offset = 0}, // normal
                            },
                },
        .descriptorSetLayouts = {scene_descriptor_layout},
        .pushConstants = {vk::PushConstantRange{vk::ShaderStageFlagBits::eVertex, 0, sizeof(ShaderParamsPushConstants)}},
        .attachments = {.depthFormat = ShadowCaster::DepthFormat},
        .depth = {.biasEnabled = true, .clampEnabled = false},
        .cull = {.mode = vk::CullModeFlagBits::eNone},
        .dynamic = {.depthBias = true}
    };

    mPipeline = createGraphicsPipeline(device, pipeline_config, {*vert_sh});
}
