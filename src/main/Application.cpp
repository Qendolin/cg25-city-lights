#include "Application.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/fast_trigonometry.hpp>

#include "RenderSystem.h"
#include "backend/Swapchain.h"
#include "backend/VulkanContext.h"
#include "debug/Performance.h"
#include "debug/SettingsGui.h"
#include "entity/Camera.h"
#include "entity/ShadowCaster.h"
#include "glfw/Input.h"
#include "imgui/ImGui.h"
#include "scene/Gltf.h"
#include "scene/Scene.h"
#include "util/Logger.h"

Application::Application() = default;
Application::~Application() = default;


void Application::processInput() {
    if (input->isKeyPress(GLFW_KEY_F5)) {
        Logger::info("Reloading render system");
        context->device().waitIdle();
        try {
            renderSystem->recreate();
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

void Application::init() {
    context = std::make_unique<VulkanContext>(std::move(VulkanContext::create(glfw::WindowCreateInfo{
        .width = 1600,
        .height = 900,
        .title = "City Lights",
        .resizable = true,
        .visible = false,
    })));
    Logger::info("Using present mode: " + vk::to_string(context->swapchain().presentMode()));
    context->window().centerOnScreen();
    glfwShowWindow(context->window());

    // imgui must be initialized after input
    input = std::make_unique<glfw::Input>(context->window());

    renderSystem = std::make_unique<RenderSystem>(context.get());

    settingsGui = std::make_unique<SettingsGui>();

    gltf::Loader gltf_loader = {};
    scene::Loader scene_loader = {
        &gltf_loader,
        context->allocator(),
        context->device(),
        context->physicalDevice(),
        context->transferQueue,
        context->mainQueue,
        renderSystem->descriptorAllocator()
    };

    scene = std::make_unique<scene::Scene>(std::move(scene_loader.load("resources/scenes/CityTest.glb")));
    for (const auto &config: settings.shadowCascades) {
        sunShadowCasterCascades.emplace_back(
                context->device(), context->allocator(), config.resolution, config.dimension, config.start, config.end
        );
    }
    camera = std::make_unique<Camera>(glm::radians(90.0f), 0.001f, glm::vec3{0, 1, 5}, glm::vec3{});
    debugFrameTimes = std::make_unique<FrameTimes>();

    renderSystem->recreate();

    blobModel = std::make_unique<blob::Model>(context->allocator(), BLOB_RESOLUTION);
}

void Application::run() {
    while (!context->window().shouldClose()) {
        renderSystem->advance();

        input->update();
        processInput();

        renderSystem->begin();
        renderSystem->imGuiBackend().beginFrame();

        drawGui();

        camera->setViewport(context->swapchain().width(), context->swapchain().height());

        // Should probably move this somewhere else
        for (size_t i = 0; i < sunShadowCasterCascades.size(); i++) {
            auto &caster = sunShadowCasterCascades[i];
            const auto &config = settings.shadowCascades[i];
            caster.lookAt(camera->position, -settings.sun.direction());
            config.applyTo(caster);
        }

        blobModel->advanceTime(input->timeDelta());

        renderSystem->draw({
            .gltfScene = scene->gpu(),
            .camera = *camera,
            .sunShadowCasterCascades = sunShadowCasterCascades,
            .sunLight = settings.sun,
            .settings = settings,
            .blobModel = *blobModel,
        });

        renderSystem->submit();
    }
    context->device().waitIdle();
}
