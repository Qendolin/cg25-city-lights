#include "ShadowRenderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../backend/ShaderCompiler.h"
#include "../entity/ShadowCaster.h"
#include "../scene/Scene.h"


ShadowRenderer::~ShadowRenderer() = default;
ShadowRenderer::ShadowRenderer() = default;

void ShadowRenderer::execute(const vk::CommandBuffer &cmd_buf, const scene::GpuData &gpu_data, const ShadowCaster &shadow_caster) {
    shadow_caster.framebuffer().depthAttachment.barrier(cmd_buf, ImageResourceAccess::DepthAttachmentWrite);

    const Framebuffer &fb = shadow_caster.framebuffer();
    cmd_buf.beginRendering(fb.renderingInfo({
        .enabledColorAttachments = {true},
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
    cmd_buf.bindVertexBuffers(
            0, {*gpu_data.positions, *gpu_data.normals, *gpu_data.tangents, *gpu_data.texcoords}, {0, 0, 0, 0}
    );

    ShaderParamsPushConstants shader_params = {
        .projectionViewMatrix = shadow_caster.projectionMatrix() * shadow_caster.viewMatrix(),
        .sizeBias = shadow_caster.extrusionBias / static_cast<float>(shadow_caster.resolution()),
    };
    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(shader_params), &shader_params);

    cmd_buf.drawIndexedIndirect(*gpu_data.drawCommands, 0, gpu_data.drawCommandCount, sizeof(vk::DrawIndexedIndirectCommand));

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
        .cull = {.front = vk::FrontFace::eClockwise},
        .dynamic = {.depthBias = true}
    };

    mPipeline = createGraphicsPipeline(device, pipeline_config, {*vert_sh});
}
