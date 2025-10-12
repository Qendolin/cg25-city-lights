#include "Application.h"

#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include "backend/Image.h"
#include "backend/ShaderCompiler.h"
#include "backend/StagingBuffer.h"
#include "backend/Swapchain.h"
#include "scene/GltfLoader.h"
#include "util/Logger.h"

void Application::createPerFrameResources() {
    const auto &swapchain = mContext->swapchain();
    const auto &device = mContext->device();
    const auto &cmd_pool = *mData.commandPool;
    auto &frame_resources = mData.perFrameResources;

    for (int i = 0; i < swapchain.imageCount(); i++) {
        auto &frame = frame_resources[i];
        frame = {
            .availableSemaphore = device.createSemaphoreUnique({}),
            .finishedSemaphore = device.createSemaphoreUnique({}),
            .inFlightFence = device.createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}),
            .descriptorSet = mData.descriptorAllocator.allocate(mData.perFrameDescriptorLayout)
        };
        frame.commandBuffer = device.allocateCommandBuffers({.commandPool = cmd_pool,
                                                             .level = vk::CommandBufferLevel::ePrimary,
                                                             .commandBufferCount = 1})
                                      .at(0);
    }
}

void Application::createImGuiBackend() {
    mData.imguiBackend = std::make_unique<ImGuiBackend>(
            mContext->instance(), mContext->device(), mContext->physicalDevice(), mContext->window(),
            mContext->swapchain(), mContext->mainQueue, mContext->swapchain().depthFormat()
    );
}

void Application::createPipeline(const ShaderLoader &loader) {
    const auto &device = mContext->device();
    auto vert_sh = loader.loadFromSource(device, "resources/shaders/pbr.vert");
    auto frag_sh = loader.loadFromSource(device, "resources/shaders/pbr.frag");

    mData.sceneDataDescriptorLayout = SceneDataDescriptorLayout(device);
    mData.perFrameDescriptorLayout = PerFrameDescriptorLayout(device);

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
        .descriptorSetLayouts = {mData.sceneDataDescriptorLayout, mData.perFrameDescriptorLayout},
        .pushConstants = {},
        .attachments =
                {
                    .colorFormats = {mContext->swapchain().colorFormatSrgb()},
                    .depthFormat = mContext->swapchain().depthFormat(),
                },
    };

    mData.pipeline = createGraphicsPipeline(mContext->device(), pipeline_config, {*vert_sh, *frag_sh});
}

SceneRenderData Application::uploadSceneData(const SceneData &scene_data) {
    SceneRenderData result;
    const vma::Allocator &allocator = mContext->allocator();
    StagingBuffer staging = {allocator, mContext->device(), *mData.transientTransferCommandPool};

    std::tie(result.positions, result.positionsAlloc) =
            staging.upload(scene_data.vertex_position_data, vk::BufferUsageFlagBits::eVertexBuffer);
    std::tie(result.normals, result.normalsAlloc) =
            staging.upload(scene_data.vertex_normal_data, vk::BufferUsageFlagBits::eVertexBuffer);
    std::tie(result.tangents, result.tangentsAlloc) =
            staging.upload(scene_data.vertex_tangent_data, vk::BufferUsageFlagBits::eVertexBuffer);
    std::tie(result.texcoords, result.texcoordsAlloc) =
            staging.upload(scene_data.vertex_texcoord_data, vk::BufferUsageFlagBits::eVertexBuffer);
    std::tie(result.indices, result.indicesAlloc) =
            staging.upload(scene_data.index_data, vk::BufferUsageFlagBits::eIndexBuffer);

    std::vector<InstanceBlock> instance_blocks;
    instance_blocks.reserve(scene_data.instances.size());
    std::vector<vk::DrawIndexedIndirectCommand> draw_commands;
    draw_commands.reserve(scene_data.instances.size());
    for (size_t i = 0; i < scene_data.instances.size(); i++) {
        const auto &instance = scene_data.instances[i];
        draw_commands.emplace_back() = vk::DrawIndexedIndirectCommand{
            .indexCount = instance.indexCount,
            .instanceCount = 1,
            .firstIndex = instance.indexOffset,
            .vertexOffset = instance.vertexOffset,
            .firstInstance = static_cast<uint32_t>(i),
        };
        instance_blocks.emplace_back() = {.transform = instance.transform, .material = instance.material};
    }

    std::tie(result.drawCommands, result.drawCommandsAlloc) =
            staging.upload(draw_commands, vk::BufferUsageFlagBits::eIndirectBuffer);
    std::tie(result.instances, result.instancesAlloc) =
            staging.upload(instance_blocks, vk::BufferUsageFlagBits::eStorageBuffer);

    std::vector<MaterialBlock> material_blocks;
    material_blocks.reserve(scene_data.materials.size());
    for (const auto &material: scene_data.materials) {
        material_blocks.emplace_back() = {
            .albedoFactors = material.albedoFactor,
            .mrnFactors = glm::vec4{material.metalnessFactor, material.roughnessFactor, material.normalFactor, 1.0f},
        };
    }
    std::tie(result.materials, result.materialsAlloc) =
            staging.upload(material_blocks, vk::BufferUsageFlagBits::eStorageBuffer);

    staging.submit(mContext->transferQueue);

    return result;
}

void Application::recordCommands(const vk::CommandBuffer &cmd_buf, const PerFrameResources &per_frame) {
    const auto &swapchain = mContext->swapchain();
    cmd_buf.begin(vk::CommandBufferBeginInfo{});

    Framebuffer fb = {};
    fb.colorAttachments = {{
        .image = swapchain.colorImage(),
        .view = swapchain.colorViewSrgb(),
        .format = swapchain.colorFormatSrgb(),
        .range = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1},
    }};
    fb.depthAttachment = {
        .image = mContext->swapchain().depthImage(),
        .view = mContext->swapchain().depthView(),
        .format = mContext->swapchain().depthFormat(),
        .range = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1},
    };

    // Main render pass
    {
        cmd_buf.beginRendering(fb.renderingInfo(
                swapchain.area(),
                {
                    .enabledColorAttachments = {true},
                    .enableDepthAttachment = true,
                    .enableStencilAttachment = false,
                    .colorLoadOps = {vk::AttachmentLoadOp::eClear},
                    .colorStoreOps = {vk::AttachmentStoreOp::eStore},
                    .depthLoadOp = vk::AttachmentLoadOp::eClear,
                }
        ));

        mData.pipeline.config.viewports = {
            // Flip viewport y axis to be like opengl
            {vk::Viewport{0.0f, swapchain.height(), swapchain.width(), -swapchain.height(), 0.0f, 1.0f}}
        };
        mData.pipeline.config.scissors = {{swapchain.area()}};
        mData.pipeline.config.apply(cmd_buf);

        cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, *mData.pipeline.pipeline);
        cmd_buf.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics, *mData.pipeline.layout, 0, {mData.sceneDataDescriptorSet, per_frame.descriptorSet}, {}
        );
        cmd_buf.bindIndexBuffer(*mData.sceneRenderData.indices, 0, vk::IndexType::eUint32);
        cmd_buf.bindVertexBuffers(
                0,
                {*mData.sceneRenderData.positions, *mData.sceneRenderData.normals, *mData.sceneRenderData.tangents,
                 *mData.sceneRenderData.texcoords},
                {0, 0, 0, 0}
        );

        cmd_buf.drawIndexedIndirect(*mData.sceneRenderData.drawCommands, 0, 1, sizeof(vk::DrawIndexedIndirectCommand));

        cmd_buf.endRendering();
    }


    // ImGui render pass
    {
        fb.colorAttachments[0].view = swapchain.colorViewLinear();
        cmd_buf.beginRendering(fb.renderingInfo(swapchain.area(), {}));
        mData.imguiBackend->render(cmd_buf);
        cmd_buf.endRendering();
    }

    fb.colorAttachments[0].barrier(cmd_buf, ImageResourceAccess::PresentSrc);
    cmd_buf.end();
}

void Application::drawGui() { ImGui::ShowDemoWindow(); }

void Application::drawFrame(uint32_t frame_index) {
    PerFrameResources &per_frame = mData.perFrameResources.at(frame_index % mData.perFrameResources.size());
    const auto &device = mContext->device();
    auto &swapchain = mContext->swapchain();

    while (device.waitForFences(*per_frame.inFlightFence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }

    if (!swapchain.advance(*per_frame.availableSemaphore)) {
        // Swapchain was re-created, skip this frame
        return;
    }

    mData.imguiBackend->beginFrame();

    drawGui();

    glm::vec3 camera_position = glm::vec3{std::sin(glfwGetTime()), 1, std::cos(glfwGetTime())} * 5.0f;
    SceneInlineUniformBlock uniform_block = {
        .view = glm::lookAt(camera_position, {0, 0, 0}, {0, 1, 0}),
        .projection = glm::perspective(glm::radians(90.0f), swapchain.width() / swapchain.height(), 0.1f, 100.0f),
        .camera = {camera_position, 0},
    };
    device.updateDescriptorSets(
            {per_frame.descriptorSet.write(
                    PerFrameDescriptorLayout::SceneUniforms, {.dataSize = sizeof(uniform_block), .pData = &uniform_block}
            )},
            {}
    );

    per_frame.commandBuffer.reset();
    recordCommands(per_frame.commandBuffer, per_frame);

    vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info = vk::SubmitInfo()
                                         .setCommandBuffers(per_frame.commandBuffer)
                                         .setWaitSemaphores(*per_frame.availableSemaphore)
                                         .setWaitDstStageMask(pipe_stage_flags)
                                         .setSignalSemaphores(*per_frame.finishedSemaphore);

    device.resetFences(*per_frame.inFlightFence);

    mContext->mainQueue->submit({submit_info}, *per_frame.inFlightFence);

    swapchain.present(mContext->presentQueue, vk::PresentInfoKHR().setWaitSemaphores(*per_frame.finishedSemaphore));
}

void Application::init() {
    mContext = std::make_unique<VulkanContext>(std::move(VulkanContext::create(glfw::WindowCreateInfo{
        .width = 1024,
        .height = 1024,
        .title = "Vulkan Triangle",
        .resizable = true,
    })));
    Logger::info("Using present mode: " + vk::to_string(mContext->swapchain().presentMode()));

    mData.commandPool = mContext->device().createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = mContext->mainQueue,
    });
    mData.transientTransferCommandPool = mContext->device().createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eTransient,
        .queueFamilyIndex = mContext->transferQueue,
    });
    mData.descriptorAllocator = DescriptorAllocator(mContext->device());

    createImGuiBackend();

    ShaderLoader shader_loader = {};
    shader_loader.optimize = true;
    shader_loader.debug = true;
    createPipeline(shader_loader);

    GltfLoader gltf_loader = {};
    SceneData scene_data = gltf_loader.load("resources/scenes/DefaultCube.glb");
    mData.sceneRenderData = uploadSceneData(scene_data);
    mData.sceneDataDescriptorSet = mData.descriptorAllocator.allocate(mData.sceneDataDescriptorLayout);
    mContext->device().updateDescriptorSets(
            {
                mData.sceneDataDescriptorSet.write(
                        SceneDataDescriptorLayout::InstanceBuffer,
                        vk::DescriptorBufferInfo{.buffer = *mData.sceneRenderData.instances, .offset = 0, .range = vk::WholeSize}
                ),
                mData.sceneDataDescriptorSet.write(
                        SceneDataDescriptorLayout::MaterialBuffer,
                        vk::DescriptorBufferInfo{.buffer = *mData.sceneRenderData.materials, .offset = 0, .range = vk::WholeSize}
                ),
            },
            {}
    );

    mData.perFrameResources.resize(mContext->swapchain().imageCount());
    createPerFrameResources();
}

void Application::run() {
    uint32_t frame_index = 0;
    while (!mContext->window().shouldClose()) {
        glfwPollEvents();
        drawFrame(frame_index);
        frame_index++;
    }
    mContext->device().waitIdle();
}
