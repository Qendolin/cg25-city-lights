#include "Application.h"

#include <GLFW/glfw3.h>

#include "backend/Image.h"
#include "backend/ShaderCompiler.h"
#include "backend/Swapchain.h"
#include "backend/VulkanContext.h"
#include "imgui/ImGui.h"
#include "renderer/PbrSceneRenderer.h"
#include "scene/Gltf.h"
#include "scene/Scene.h"
#include "util/Logger.h"

Application::Application() = default;
Application::~Application() = default;

void Application::createPerFrameResources() {
    const auto &swapchain = context->swapchain();
    const auto &device = context->device();
    const auto &cmd_pool = *commandPool;

    syncObjects.create(swapchain.imageCount(), [&] {
        return SyncObjects{
            .availableSemaphore = device.createSemaphoreUnique({}),
            .finishedSemaphore = device.createSemaphoreUnique({}),
            .inFlightFence = device.createFenceUnique({.flags = vk::FenceCreateFlagBits::eSignaled}),
        };
    });
    commandBuffers.create(swapchain.imageCount(), [&] {
        return device
                .allocateCommandBuffers(
                        {.commandPool = cmd_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1}
                )
                .at(0);
    });
    swapchainFramebuffers.create(swapchain.imageCount(), [&](int i) {
        auto fb = Framebuffer(swapchain.area());
        fb.colorAttachments = {{
            .image = swapchain.colorImage(i),
            .view = swapchain.colorViewSrgb(i),
            .format = swapchain.colorFormatSrgb(),
            .range = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1},
        }};
        fb.depthAttachment = {
            .image = context->swapchain().depthImage(),
            .view = context->swapchain().depthView(),
            .format = context->swapchain().depthFormat(),
            .range = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1},
        };
        return fb;
    });
}

void Application::recordCommands(const vk::CommandBuffer &cmd_buf, Framebuffer &fb) const {
    const auto &swapchain = context->swapchain();

    pbrSceneRenderer->prepare(context->device(), fb, scene->gpu());

    cmd_buf.begin(vk::CommandBufferBeginInfo{});

    // Main render pass
    pbrSceneRenderer->render(cmd_buf, fb, scene->gpu());

    // ImGui render pass
    {
        // temporarily change view to linear format to fix an ImGui issue.
        Framebuffer imgui_fb = fb;
        imgui_fb.colorAttachments[0].view = swapchain.colorViewLinear();
        cmd_buf.beginRendering(imgui_fb.renderingInfo({}));
        imguiBackend->render(cmd_buf);
        cmd_buf.endRendering();
    }

    fb.colorAttachments[0].barrier(cmd_buf, ImageResourceAccess::PresentSrc);
    cmd_buf.end();
}

void Application::drawGui() { ImGui::ShowDemoWindow(); }

void Application::drawFrame() {
    SyncObjects &sync_objects = syncObjects.next();
    vk::CommandBuffer &cmd_buf = commandBuffers.next();
    Framebuffer &fb = swapchainFramebuffers.next();
    const auto &device = context->device();
    auto &swapchain = context->swapchain();

    while (device.waitForFences(*sync_objects.inFlightFence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }

    if (!swapchain.advance(*sync_objects.availableSemaphore)) {
        // Swapchain was re-created, skip this frame
        return;
    }

    imguiBackend->beginFrame();
    drawGui();

    cmd_buf.reset();
    recordCommands(cmd_buf, fb);

    vk::PipelineStageFlags pipe_stage_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info = vk::SubmitInfo()
                                         .setCommandBuffers(cmd_buf)
                                         .setWaitSemaphores(*sync_objects.availableSemaphore)
                                         .setWaitDstStageMask(pipe_stage_flags)
                                         .setSignalSemaphores(*sync_objects.finishedSemaphore);

    device.resetFences(*sync_objects.inFlightFence);

    context->mainQueue->submit({submit_info}, *sync_objects.inFlightFence);

    swapchain.present(context->presentQueue, vk::PresentInfoKHR().setWaitSemaphores(*sync_objects.finishedSemaphore));
}

void Application::init() {
    context = std::make_unique<VulkanContext>(std::move(VulkanContext::create(glfw::WindowCreateInfo{
        .width = 1024,
        .height = 1024,
        .title = "Vulkan Triangle",
        .resizable = true,
    })));
    Logger::info("Using present mode: " + vk::to_string(context->swapchain().presentMode()));

    imguiBackend = std::make_unique<ImGuiBackend>(
            context->instance(), context->device(), context->physicalDevice(), context->window(), context->swapchain(),
            context->mainQueue, context->swapchain().depthFormat()
    );

    commandPool = context->device().createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = context->mainQueue,
    });
    transientTransferCommandPool = context->device().createCommandPoolUnique({
        .flags = vk::CommandPoolCreateFlagBits::eTransient,
        .queueFamilyIndex = context->transferQueue,
    });
    descriptorAllocator = std::make_unique<DescriptorAllocator>(context->device());

    gltf::Loader gltf_loader = {};
    scene::Loader scene_loader = {
        &gltf_loader, context->allocator(), context->device(), *transientTransferCommandPool, context->transferQueue
    };
    scene = std::make_unique<scene::Scene>(std::move(scene_loader.load("resources/scenes/ComplexTest.glb")));

    ShaderLoader shader_loader = {};
    shader_loader.optimize = true;
    shader_loader.debug = true;
    pbrSceneRenderer = std::make_unique<PbrSceneRenderer>(
            context->device(), *descriptorAllocator, shader_loader, context->swapchain()
    );

    createPerFrameResources();
}

void Application::run() {
    while (!context->window().shouldClose()) {
        glfwPollEvents();
        drawFrame();
    }
    context->device().waitIdle();
}
