#include "Application.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/fast_trigonometry.hpp>
#include <vulkan/vulkan.hpp>

#include "RenderSystem.h"
#include "backend/Swapchain.h"
#include "backend/VulkanContext.h"
#include "debug/Performance.h"
#include "debug/SettingsGui.h"
#include "entity/Camera.h"
#include "entity/Cubemap.h"
#include "entity/ShadowCaster.h"
#include "glfw/Input.h"
#include "imgui/ImGui.h"
#include "scene/AnimationSampler.h"
#include "scene/Gltf.h"
#include "scene/Loader.h"
#include "scene/Scene.h"
#include "util/Logger.h"

Application::Application() {
    ctx = std::make_unique<VulkanContext>(std::move(VulkanContext::create(glfw::WindowCreateInfo{
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
        .title = TITLE,
        .resizable = true,
        .visible = false,
    })));

    Logger::info("Using present mode: " + vk::to_string(ctx->swapchain().presentMode()));

    ctx->window().centerOnScreen();
    glfwShowWindow(ctx->window());

    // imgui must be initialized after input
    input = std::make_unique<glfw::Input>(ctx->window());
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(ctx->window(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    render_system = std::make_unique<RenderSystem>(ctx.get());

    settings_gui = std::make_unique<SettingsGui>();

    scene::Loader scene_loader{
        ctx->allocator(), ctx->device(), ctx->physicalDevice(), ctx->transferQueue, ctx->mainQueue,
    };

    scene = std::make_unique<scene::Scene>(std::move(scene_loader.load(SCENE_FILENAME)));
    sun_shadow_cascade = std::make_unique<ShadowCascade>(
            ctx->device(), ctx->allocator(), settings.shadowCascade.resolution, Settings::SHADOW_CASCADE_COUNT
    );

    blob_model = std::make_unique<blob::Model>(ctx->allocator(), ctx->device(), BLOB_RESOLUTION);
    skybox = std::make_unique<Cubemap>(ctx->allocator(), ctx->device(), ctx->transferQueue, ctx->mainQueue, SKYBOX_FILENAMES);

    camera = std::make_unique<Camera>(FOV, NEAR_PLANE, CAMERA_POSITION, glm::vec3{});
    debug_frame_times = std::make_unique<FrameTimes>();

    render_system->recreate(settings);
}

Application::~Application() = default;

void Application::run() {
    std::vector<scene::InstanceAnimationCursor> animation_cursor_cache(scene->cpu().instance_animations.size());

    while (!ctx->window().shouldClose()) {
        render_system->advance(settings);

        input->update();
        processInput();
        blob_model->advanceTime(input->timeDelta());

        render_system->begin();
        render_system->imGuiBackend().beginFrame();

        drawGui();

        camera->setViewport(ctx->swapchain().width(), ctx->swapchain().height());
        updateSunShadowCascades();

        render_system->draw({
            .gltfScene = scene->gpu(),
            .camera = *camera,
            .sunShadowCasterCascade = *sun_shadow_cascade,
            .sunLight = settings.sun,
            .settings = settings,
            .blobModel = *blob_model,
            .skybox = *skybox,
        });

        render_system->submit(settings);

        std::vector<glm::mat4> animated_instance_transforms = scene::AnimationSampler::sampleAnimatedInstanceTransforms(
                scene->cpu(), static_cast<float>(input->time()) - 4.f, animation_cursor_cache
        );
    }

    ctx->device().waitIdle();
}

void Application::processInput() {
    if (input->isKeyPress(GLFW_KEY_F5))
        reloadRenderSystem();

    updateMouseCapture();

    if (input->isMouseCaptured()) {
        updateCamera();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}

void Application::drawGui() {
    debug_frame_times->update(input->timeDelta());
    debug_frame_times->draw();

    settings_gui->draw(settings);
}

void Application::updateSunShadowCascades() {
    sun_shadow_cascade->lambda = settings.shadowCascade.lambda;
    sun_shadow_cascade->distance = settings.shadowCascade.distance;
    sun_shadow_cascade->update(camera->fov(), camera->aspect(), camera->viewMatrix(), -settings.sun.direction());

    for (size_t i = 0; i < settings.shadowCascades.size(); i++)
        settings.shadowCascades[i].applyTo(sun_shadow_cascade->cascades()[i]);
}

void Application::reloadRenderSystem() {
    Logger::info("Reloading render system");

    ctx->device().waitIdle();

    try {
        render_system->recreate(settings);
    } catch (const std::exception &exc) {
        Logger::error("Reload failed: " + std::string(exc.what()));
    }
}

void Application::updateMouseCapture() {
    if (input->isMouseReleased() && input->isMousePress(GLFW_MOUSE_BUTTON_LEFT)) {
        if (!ImGui::GetIO().WantCaptureMouse)
            input->captureMouse();
    } else if (input->isMouseCaptured() && (input->isKeyPress(GLFW_KEY_ESCAPE) || input->isKeyPress(GLFW_KEY_LEFT_ALT)))
        input->releaseMouse();
}

void Application::updateCamera() {
    // Yaw
    camera->angles.y -= input->mouseDelta().x * MOUSE_SENSITIVITY;
    camera->angles.y = glm::wrapAngle(camera->angles.y);

    // Pitch
    camera->angles.x -= input->mouseDelta().y * MOUSE_SENSITIVITY;
    camera->angles.x = glm::clamp(camera->angles.x, -glm::half_pi<float>(), glm::half_pi<float>());

    glm::vec3 move_input = {
        input->isKeyDown(GLFW_KEY_D) - input->isKeyDown(GLFW_KEY_A),
        input->isKeyDown(GLFW_KEY_SPACE) - input->isKeyDown(GLFW_KEY_LEFT_CONTROL),
        input->isKeyDown(GLFW_KEY_S) - input->isKeyDown(GLFW_KEY_W)
    };

    glm::vec3 velocity = move_input * BASE_SPEED;
    velocity = glm::mat3(glm::rotate(glm::mat4(1.0f), camera->angles.y, {0, 1, 0})) * velocity;

    if (input->isKeyDown(GLFW_KEY_LEFT_SHIFT))
        velocity *= FAST_SPEED_MULTIPLIER;

    camera->position += velocity * input->timeDelta();
    camera->updateViewMatrix();
}
