#include "PbrSceneRenderer.h"

#include <glm/ext/matrix_clip_space.hpp>

#include "../backend/Framebuffer.h"
#include "../backend/ShaderCompiler.h"
#include "../entity/Camera.h"
#include "../entity/Light.h"
#include "../entity/ShadowCaster.h"
#include "../scene/Scene.h"
#include "../util/Logger.h"
#include "FrustumCuller.h"

PbrSceneRenderer::~PbrSceneRenderer() = default;

PbrSceneRenderer::PbrSceneRenderer(const vk::Device &device) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);
    mShadowSampler = device.createSamplerUnique({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToBorder,
        .addressModeV = vk::SamplerAddressMode::eClampToBorder,
        .compareEnable = true,
        .compareOp = vk::CompareOp::eGreaterOrEqual,
        .borderColor = vk::BorderColor::eFloatTransparentBlack,
    });
}

void PbrSceneRenderer::recreate(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb) {
    createPipeline(device, shader_loader, fb);
}

void PbrSceneRenderer::execute(
        const vk::Device &device,
        const DescriptorAllocator &desc_alloc,
        const TransientBufferAllocator &buf_alloc,
        const vk::CommandBuffer &cmd_buf,
        const Framebuffer &fb,
        const Camera &camera,
        const scene::GpuData &gpu_data,
        const FrustumCuller& frustum_culler,
        const DirectionalLight &sun_light,
        std::span<ShadowCaster> sun_shadow_cascades,
        const glm::vec3 &ambient_light
) {

    Logger::check(
            sun_shadow_cascades.size() == ShaderParamsInlineUniformBlock{}.cascades.size(),
            "Shadow cascade size doesn't match"
    );

    // Culling

    glm::mat4 frustum_matrix = camera.projectionMatrix() * camera.viewMatrix();
    if (pauseCulling) {
        if (!mCapturedFrustum.has_value()) {
            mCapturedFrustum = std::make_optional<glm::mat4>(frustum_matrix);
        }
        frustum_matrix = mCapturedFrustum.value();
    } else if (mCapturedFrustum.has_value()) {
        mCapturedFrustum.reset();
    }

    BufferRef culled_commands = {};
    if (enableCulling) {
        culled_commands = frustum_culler.execute(device, desc_alloc, buf_alloc, cmd_buf, gpu_data, frustum_matrix);
        culled_commands.barrier(cmd_buf, BufferResourceAccess::IndirectCommandRead);
    }

    // Rendering

    ShaderParamsInlineUniformBlock uniform_block = {
        .view = camera.viewMatrix(),
        .projection = camera.projectionMatrix(),
        .camera = {camera.position, 0},
        .sun =
                {
                    .radiance = glm::vec4{sun_light.radiance(), 0.0},
                    .direction = glm::vec4{sun_light.direction(), 0.0},
                },
        .ambient = glm::vec4(ambient_light, 1.0),
    };
    for (size_t i = 0; i < uniform_block.cascades.size(); i++) {
        const auto &cascade = sun_shadow_cascades[i];
        // Divide by resolution to help keep the bias resolution independent
        float normal_bias = cascade.normalBias / static_cast<float>(cascade.resolution());
        uniform_block.cascades[i] = {
            .projectionView = cascade.projectionMatrix() * cascade.viewMatrix(),
            .sampleBias = cascade.sampleBias,
            .sampleBiasClamp = cascade.sampleBiasClamp,
            .normalBias = normal_bias,
            .dimension = cascade.dimension
        };
    }

    auto descriptor_set = desc_alloc.allocate(mShaderParamsDescriptorLayout);
    device.updateDescriptorSets(
            descriptor_set.write(
                    ShaderParamsDescriptorLayout::SceneUniforms, {.dataSize = sizeof(uniform_block), .pData = &uniform_block}
            ),
            {}
    );
    for (size_t i = 0; i < sun_shadow_cascades.size(); i++) {
        device.updateDescriptorSets(
                descriptor_set.write(
                        ShaderParamsDescriptorLayout::SunShadowMap,
                        vk::DescriptorImageInfo{
                            .sampler = *mShadowSampler,
                            .imageView = sun_shadow_cascades[i].framebuffer().depthAttachment.view,
                            .imageLayout = vk::ImageLayout::eDepthReadOnlyOptimal
                        },
                        i
                ),
                {}
        );
        sun_shadow_cascades[i].framebuffer().depthAttachment.barrier(cmd_buf, ImageResourceAccess::FragmentShaderReadOptimal);
    }


    cmd_buf.beginRendering(fb.renderingInfo({
        .enabledColorAttachments = {true},
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eLoad},
        .colorStoreOps = {vk::AttachmentStoreOp::eStore},
        .depthLoadOp = vk::AttachmentLoadOp::eClear,
    }));

    mPipeline.config.viewports = {{fb.viewport(true)}};
    mPipeline.config.scissors = {{fb.area()}};
    mPipeline.config.apply(cmd_buf);

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline.pipeline);
    cmd_buf.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *mPipeline.layout, 0, {gpu_data.sceneDescriptor, descriptor_set}, {}
    );
    cmd_buf.bindIndexBuffer(*gpu_data.indices, 0, vk::IndexType::eUint32);
    cmd_buf.bindVertexBuffers(
            0, {*gpu_data.positions, *gpu_data.normals, *gpu_data.tangents, *gpu_data.texcoords}, {0, 0, 0, 0}
    );

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

void PbrSceneRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Framebuffer &fb) {
    auto vert_sh = shader_loader.loadFromSource(device, "resources/shaders/pbr.vert");
    auto frag_sh = shader_loader.loadFromSource(device, "resources/shaders/pbr.frag");

    auto scene_descriptor_layout = scene::SceneDescriptorLayout(device);
    GraphicsPipelineConfig pipeline_config = {
        .vertexInput =
                {
                    .bindings =
                            {
                                {.binding = 0, .stride = sizeof(glm::vec3), .inputRate = vk::VertexInputRate::eVertex}, // position
                                {.binding = 1, .stride = sizeof(glm::vec3), .inputRate = vk::VertexInputRate::eVertex}, // normal
                                {.binding = 2, .stride = sizeof(glm::vec4), .inputRate = vk::VertexInputRate::eVertex}, // tangent
                                {.binding = 3, .stride = sizeof(glm::vec2), .inputRate = vk::VertexInputRate::eVertex}, // texcoord
                            },
                    .attributes =
                            {
                                {.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = 0}, // position
                                {.location = 1, .binding = 1, .format = vk::Format::eR32G32B32Sfloat, .offset = 0}, // normal
                                {.location = 2, .binding = 2, .format = vk::Format::eR32G32B32A32Sfloat, .offset = 0}, // tangent
                                {.location = 3, .binding = 3, .format = vk::Format::eR32G32Sfloat, .offset = 0}, // texcoord
                            },
                },
        .descriptorSetLayouts = {scene_descriptor_layout, mShaderParamsDescriptorLayout},
        .pushConstants = {},
        .attachments =
                {
                    .colorFormats = fb.colorFormats(),
                    .depthFormat = fb.depthFormat(),
                },
    };

    mPipeline = createGraphicsPipeline(device, pipeline_config, {*vert_sh, *frag_sh});
}
