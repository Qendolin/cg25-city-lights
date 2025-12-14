#include "DepthPrePassRenderer.h"

#include <glm/ext/matrix_clip_space.hpp>

#include "../backend/Framebuffer.h"
#include "../backend/ShaderCompiler.h"
#include "../debug/Annotation.h"
#include "../entity/Camera.h"
#include "../scene/Scene.h"
#include "FrustumCuller.h"

DepthPrePassRenderer::~DepthPrePassRenderer() = default;

DepthPrePassRenderer::DepthPrePassRenderer() = default;

void DepthPrePassRenderer::recreate(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb) {
    createPipeline(device, shader_loader, fb);
}

void DepthPrePassRenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &desc_alloc,
        const TransientBufferAllocator &buf_alloc,
        const vk::CommandBuffer &cmd_buf,
        const Framebuffer &fb,
        const Camera &camera,
        const scene::GpuData &gpu_data,
        const FrustumCuller &frustum_culler
) {
    util::ScopedCommandLabel dbg_cmd_label_region_culling(cmd_buf, "Culling");

    glm::mat4 frustum_matrix = camera.projectionMatrix() * camera.viewMatrix();
    if (pauseCulling) {
        if (!mCapturedFrustum.has_value()) {
            mCapturedFrustum = std::make_optional<glm::mat4>(frustum_matrix);
        }
        frustum_matrix = mCapturedFrustum.value();
    } else if (mCapturedFrustum.has_value()) {
        mCapturedFrustum.reset();
    }

    UnmanagedBuffer culled_commands = {};
    if (enableCulling) {
        culled_commands = frustum_culler.execute(device, desc_alloc, buf_alloc, cmd_buf, gpu_data, frustum_matrix);
        culled_commands.barrier(cmd_buf, BufferResourceAccess::IndirectCommandRead);
    }

    dbg_cmd_label_region_culling.swap("Rendering");

    fb.depthAttachment.image().barrier(
            cmd_buf, ImageResourceAccess::DepthAttachmentEarlyOps, ImageResourceAccess::DepthAttachmentLateOps
    );

    cmd_buf.beginRendering(fb.renderingInfo({
        .enableColorAttachments = false,
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .depthLoadOp = vk::AttachmentLoadOp::eClear,
        .depthStoreOp = vk::AttachmentStoreOp::eStore,
    }));

    mPipeline.config.viewports = {{fb.viewport(true)}};
    mPipeline.config.scissors = {{fb.area()}};
    mPipeline.config.apply(cmd_buf);

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline.pipeline);

    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *mPipeline.layout, 0, {gpu_data.sceneDescriptor}, {});

    cmd_buf.bindIndexBuffer(*gpu_data.indices, 0, vk::IndexType::eUint32);
    cmd_buf.bindVertexBuffers(0, {*gpu_data.positions}, {0});

    ShaderPushConstants push_constants = {.view = camera.viewMatrix(), .projection = camera.projectionMatrix()};
    cmd_buf.pushConstants(*mPipeline.layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(push_constants), &push_constants);

    if (enableCulling) {
        cmd_buf.drawIndexedIndirectCount(
                culled_commands, 0, culled_commands, culled_commands.size - 32, gpu_data.drawCommandCount,
                sizeof(vk::DrawIndexedIndirectCommand)
        );
    } else {
        cmd_buf.drawIndexedIndirect(
                *gpu_data.drawCommands, 0, gpu_data.drawCommandCount, sizeof(vk::DrawIndexedIndirectCommand)
        );
    }
    cmd_buf.endRendering();
}

void DepthPrePassRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb) {
    auto vert_sh = shader_loader.loadFromSource(device, "resources/shaders/depth_prepass.vert");

    auto scene_descriptor_layout = scene::SceneDescriptorLayout(device);
    GraphicsPipelineConfig pipeline_config = {
        .vertexInput =
                {
                    .bindings = {{.binding = 0, .stride = sizeof(glm::vec3), .inputRate = vk::VertexInputRate::eVertex}},
                    .attributes = {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = 0}},
                },
        .descriptorSetLayouts = {scene_descriptor_layout},
        .pushConstants = {{.stageFlags = vk::ShaderStageFlagBits::eVertex, .offset = 0, .size = sizeof(ShaderPushConstants)}},
        .attachments =
                {
                    .colorFormats = {},
                    .depthFormat = fb.depthFormat(),
                },
    };

    mPipeline = createGraphicsPipeline(device, pipeline_config, {*vert_sh});
}
