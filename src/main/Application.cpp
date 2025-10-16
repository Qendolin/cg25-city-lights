#include "Application.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/fast_trigonometry.hpp>

#include "backend/Framebuffer.h"
#include "backend/ShaderCompiler.h"
#include "backend/Swapchain.h"
#include "backend/VulkanContext.h"
#include "debug/Performance.h"
#include "debug/SettingsGui.h"
#include "entity/Camera.h"
#include "glfw/Input.h"
#include "imgui/ImGui.h"
#include "renderer/PbrSceneRenderer.h"
#include "renderer/ShadowRenderer.h"
#include "scene/Gltf.h"
#include "scene/Scene.h"
#include "util/Logger.h"

Application::Application() = default;
Application::~Application() = default;

void Application::recordCommands(const vk::CommandBuffer &cmd_buf, Framebuffer &fb) const {
    const auto &swapchain = context->swapchain();

    pbrSceneRenderer->prepare(context->device(), *camera, settings.sun, *sunShadowCaster);

    cmd_buf.begin(vk::CommandBufferBeginInfo{});

    // Shadow pass
    shadowRenderer->render(cmd_buf, scene->gpu(), *sunShadowCaster);

    // Main render pass
    pbrSceneRenderer->render(cmd_buf, fb, scene->gpu(), *sunShadowCaster);

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

void Application::processInput() {
    if (input->isKeyPress(GLFW_KEY_F5)) {
        Logger::info("Reloading shaders");
        context->device().waitIdle();
        try {
            recreate();
        } catch (const std::exception &exc) {
            Logger::error("Reload failed: " + std::string(exc.what()));
        }
    }

    if (input->isMouseReleased() && input->isMousePress(GLFW_MOUSE_BUTTON_LEFT)) {
        if (!ImGui::GetIO().WantCaptureMouse)
            input->captureMouse();
    } else if (input->isMouseCaptured() && (input->isKeyPress(GLFW_KEY_ESCAPE) || input->isKeyPress(GLFW_KEY_LEFT_ALT))) {
        input->releaseMouse();
    }

    if (input->isMouseCaptured()) {
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else {
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    if (input->isMouseCaptured()) {
        // yaw
        camera->angles.y -= input->mouseDelta().x * glm::radians(0.15f);
        camera->angles.y = glm::wrapAngle(camera->angles.y);

        // pitch
        camera->angles.x -= input->mouseDelta().y * glm::radians(0.15f);
        camera->angles.x = glm::clamp(camera->angles.x, -glm::half_pi<float>(), glm::half_pi<float>());

        glm::vec3 move_input = {
            input->isKeyDown(GLFW_KEY_D) - input->isKeyDown(GLFW_KEY_A),
            input->isKeyDown(GLFW_KEY_SPACE) - input->isKeyDown(GLFW_KEY_LEFT_CONTROL),
            input->isKeyDown(GLFW_KEY_S) - input->isKeyDown(GLFW_KEY_W)
        };
        glm::vec3 velocity = move_input * 5.0f;
        velocity = glm::mat3(glm::rotate(glm::mat4(1.0), camera->angles.y, {0, 1, 0})) * velocity;
        camera->position += velocity * input->timeDelta();
    }
    camera->updateViewMatrix();
}

void Application::drawGui() {
    debugFrameTimes->update(input->timeDelta());
    debugFrameTimes->draw();

    settingsGui->draw(settings);
}

void Application::drawFrame() {
    const auto &device = context->device();
    auto &swapchain = context->swapchain();
    SyncObjects &sync_objects = syncObjects.next();
    vk::CommandBuffer &cmd_buf = commandBuffers.next();

    while (device.waitForFences(*sync_objects.inFlightFence, true, UINT64_MAX) == vk::Result::eTimeout) {
    }

    if (!swapchain.advance(*sync_objects.availableSemaphore)) {
        recreate();
        // Swapchain was re-created, skip this frame
        return;
    }
    // Framebuffer needs to be synced to swapchain, so get it explicitly
    Framebuffer &fb = swapchainFramebuffers.get(swapchain.activeImageIndex());
    camera->setViewport(swapchain.width(), swapchain.height());

    // Update input after waiting for fences
    input->update();
    processInput();

    imguiBackend->beginFrame();
    drawGui();

    // Should probably move this somewhere else
    sunShadowCaster->lookAt(camera->position, -settings.sun.direction());
    settings.shadow.applyTo(*sunShadowCaster);

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

    if (!swapchain.present(context->presentQueue, vk::PresentInfoKHR().setWaitSemaphores(*sync_objects.finishedSemaphore))) {
        recreate();
    }
}

void Application::recreate() {
    Logger::debug("Application::recreate called");
    const auto &swapchain = context->swapchain();
    const auto &device = context->device();
    const auto &cmd_pool = *commandPool;

    // I don't really like that recrate has to be called explicitly.
    // I'd prefer an implicit solution, but I couldn't think of a good one right now.
    pbrSceneRenderer->recreate(context->device(), *shaderLoader, swapchain);
    shadowRenderer->recreate(device, *shaderLoader);

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

void Application::init() {
    context = std::make_unique<VulkanContext>(std::move(VulkanContext::create(glfw::WindowCreateInfo{
        .width = 1600,
        .height = 900,
        .title = "City Lights",
        .resizable = true,
    })));
    Logger::info("Using present mode: " + vk::to_string(context->swapchain().presentMode()));
    context->window().centerOnScreen();

    input = std::make_unique<glfw::Input>(context->window());

    imguiBackend = std::make_unique<ImGuiBackend>(
            context->instance(), context->device(), context->physicalDevice(), context->window(), context->swapchain(),
            context->mainQueue, context->swapchain().depthFormat()
    );
    settingsGui = std::make_unique<SettingsGui>();

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
    scene::Loader scene_loader = {&gltf_loader,           context->allocator(),
                                  context->device(),      context->physicalDevice(),
                                  context->transferQueue, context->mainQueue,
                                  *descriptorAllocator};
    scene = std::make_unique<scene::Scene>(std::move(scene_loader.load("resources/scenes/ComplexTest.glb")));
    sunShadowCaster = std::make_unique<ShadowCaster>(context->device(), context->allocator(), settings.shadow.resolution, settings.shadow.dimension, settings.shadow.start, settings.shadow.end);

    camera = std::make_unique<Camera>(glm::radians(90.0f), 0.001f, glm::vec3{0, 1, 5}, glm::vec3{});
    debugFrameTimes = std::make_unique<FrameTimes>();

    shaderLoader = std::make_unique<ShaderLoader>();
    shaderLoader->optimize = true;
    shaderLoader->debug = true;
    pbrSceneRenderer = std::make_unique<PbrSceneRenderer>(context->device(), *descriptorAllocator, context->swapchain());
    shadowRenderer = std::make_unique<ShadowRenderer>();
    recreate();
}

void Application::run() {
    while (!context->window().shouldClose()) {
        drawFrame();
    }
    context->device().waitIdle();
}
