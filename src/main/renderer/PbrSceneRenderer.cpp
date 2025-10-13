#include "PbrSceneRenderer.h"

#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../backend/Image.h"
#include "../backend/ShaderCompiler.h"
#include "../backend/Swapchain.h"
#include "../scene/Scene.h"

PbrSceneRenderer::~PbrSceneRenderer() = default;

PbrSceneRenderer::PbrSceneRenderer(
        const vk::Device &device, const DescriptorAllocator &allocator, const ShaderLoader &shader_loader, const Swapchain &swapchain
) {
    createDescriptors(device, allocator, swapchain);
    createPipeline(device, shader_loader, swapchain);
}

void PbrSceneRenderer::prepare(const vk::Device &device, const Framebuffer &fb, const scene::GpuData &gpu_data) const {
    // This is just temporarily here
    glm::vec3 camera_position = glm::vec3{std::sin(glfwGetTime()), 1, std::cos(glfwGetTime())} * 5.0f;
    float aspect_ratio = static_cast<float>(fb.area.extent.width) / fb.area.extent.height;
    ShaderParamsInlineUniformBlock uniform_block = {
        .view = glm::lookAt(camera_position, {0, 0, 0}, {0, 1, 0}),
        .projection = glm::perspective(glm::radians(90.0f), aspect_ratio, 0.1f, 100.0f),
        .camera = {camera_position, 0},
    };
    device.updateDescriptorSets(
            {mShaderParamsDescriptors.get().write(
                    ShaderParamsDescriptorLayout::SceneUniforms, {.dataSize = sizeof(uniform_block), .pData = &uniform_block}
            )},
            {}
    );

    // Since the data is static, the descriptors don't need to be updated each frame.
    // But I found it easier to implement this way.
    gpu_data.writeDescriptorSet(device, mSceneDescriptors.get());
}

void PbrSceneRenderer::render(const vk::CommandBuffer &cmd_buf, const Framebuffer &fb, const scene::GpuData &gpu_data) {
    cmd_buf.beginRendering(fb.renderingInfo({
        .enabledColorAttachments = {true},
        .enableDepthAttachment = true,
        .enableStencilAttachment = false,
        .colorLoadOps = {vk::AttachmentLoadOp::eClear},
        .colorStoreOps = {vk::AttachmentStoreOp::eStore},
        .depthLoadOp = vk::AttachmentLoadOp::eClear,
    }));


    mPipeline.config.viewports = {{fb.viewport()}};
    mPipeline.config.scissors = {{fb.area}};
    mPipeline.config.apply(cmd_buf);

    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *mPipeline.pipeline);
    cmd_buf.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, *mPipeline.layout, 0,
            {mSceneDescriptors.next(), mShaderParamsDescriptors.next()}, {}
    );
    cmd_buf.bindIndexBuffer(*gpu_data.indices, 0, vk::IndexType::eUint32);
    cmd_buf.bindVertexBuffers(
            0, {*gpu_data.positions, *gpu_data.normals, *gpu_data.tangents, *gpu_data.texcoords}, {0, 0, 0, 0}
    );

    cmd_buf.drawIndexedIndirect(*gpu_data.drawCommands, 0, gpu_data.drawCommandCount, sizeof(vk::DrawIndexedIndirectCommand));

    cmd_buf.endRendering();
}

void PbrSceneRenderer::createDescriptors(const vk::Device &device, const DescriptorAllocator &allocator, const Swapchain &swapchain) {
    mShaderParamsDescriptorLayout = ShaderParamsDescriptorLayout(device);
    mShaderParamsDescriptors.create(swapchain.imageCount(), [&]() {
        return allocator.allocate(mShaderParamsDescriptorLayout);
    });

    mSceneDescriptorLayout = scene::SceneDescriptorLayout(device);
    mSceneDescriptors.create(swapchain.imageCount(), [&]() { return allocator.allocate(mSceneDescriptorLayout); });
}

void PbrSceneRenderer::createPipeline(const vk::Device &device, const ShaderLoader &shader_loader, const Swapchain &swapchain) {
    auto vert_sh = shader_loader.loadFromSource(device, "resources/shaders/pbr.vert");
    auto frag_sh = shader_loader.loadFromSource(device, "resources/shaders/pbr.frag");

    PipelineConfig pipeline_config = {
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
        .descriptorSetLayouts = {mSceneDescriptorLayout, mShaderParamsDescriptorLayout},
        .pushConstants = {},
        .attachments =
                {
                    .colorFormats = {swapchain.colorFormatSrgb()},
                    .depthFormat = swapchain.depthFormat(),
                },
    };

    mPipeline = createGraphicsPipeline(device, pipeline_config, {*vert_sh, *frag_sh});
}
