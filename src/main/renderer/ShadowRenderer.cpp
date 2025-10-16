#include "ShadowRenderer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../backend/ShaderCompiler.h"
#include "../scene/Scene.h"

ShadowCaster::ShadowCaster(
        const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution, float dimension, float start, float end
)
    : dimension(dimension), start(start), end(end), mResolution(resolution) {
    mDepthImage = Image::create(
            allocator,
            {
                .format = DepthFormat,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                .type = vk::ImageType::e2D,
                .width = resolution,
                .height = resolution,
                .mip_levels = 1,
            }
    );
    mDepthImageView = mDepthImage.createDefaultView(device);
    mFramebuffer = Framebuffer{vk::Extent2D{resolution, resolution}};
    mFramebuffer.depthAttachment = {
        .image = mDepthImage.image,
        .view = *mDepthImageView,
        .format = mDepthImage.info.format,
        .range = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1},
    };
    // clang-format off
}

glm::mat4 ShadowCaster::projectionMatrix() const{
    float half_extent = 0.5f * dimension;
    // swap near and far plane for reverse z
    return glm::ortho(-half_extent, half_extent, -half_extent, half_extent, end, start);
}

void ShadowCaster::lookAt(const glm::vec3 &target, const glm::vec3 &direction, float distance, const glm::vec3 &up_) {
    glm::vec3 up = up_;
    float dot = glm::dot(direction, up);
    if (dot < -0.99 || dot > 0.99) {
        // direction is too close to up vector, pick another one
        glm::vec3 abs = glm::abs(up);
        if (abs.x < abs.y && abs.x < abs.z)
            up = {1, 0, 0};
        else if (abs.y < abs.z)
            up = {0, 1, 0};
        else
            up = {0, 0, 1};
    }

    glm::vec3 eye = target - glm::normalize(direction) * distance;
    // add direction to target to make it work for distance = 0
    mViewMatrix = glm::lookAt(eye, target + direction, up);
}

void ShadowCaster::lookAt(const glm::vec3 &target, float azimuth, float elevation, float distance, const glm::vec3 &up) {
    glm::vec3 direction = glm::vec3{
        glm::sin(azimuth) * glm::cos(elevation),
        glm::sin(elevation),
        glm::cos(azimuth) * glm::cos(elevation),
    };
    lookAt(target, -direction, distance, up);
}

ShadowRenderer::~ShadowRenderer() = default;
ShadowRenderer::ShadowRenderer() = default;

void ShadowRenderer::render(const vk::CommandBuffer &cmd_buf, const scene::GpuData &gpu_data, const ShadowCaster &shadow_caster) {
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

    mPipeline.config.viewports = {{fb.viewport()}};
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
    PipelineConfig pipeline_config = {
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
        .depth = { .biasEnabled = true, .clampEnabled = false },
        .attachments = {.depthFormat = ShadowCaster::DepthFormat},
        .dynamic = {.depthBias = true}
    };

    mPipeline = createGraphicsPipeline(device, pipeline_config, {*vert_sh});
}
