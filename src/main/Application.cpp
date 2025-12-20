#include "Application.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/fast_trigonometry.hpp>
#include <vulkan/vulkan.hpp>

#include "RenderSystem.h"
#include "audio/Audio.h"
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
    mCtx = std::make_unique<VulkanContext>(std::move(VulkanContext::create(glfw::WindowCreateInfo{
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
        .title = TITLE,
        .resizable = true,
        .visible = false,
    })));
    mSettings.rendering.asyncCompute = mCtx->computeQueue.queue != VK_NULL_HANDLE;

    Logger::info("Using present mode: " + vk::to_string(mCtx->swapchain().presentMode()));

    mCtx->window().centerOnScreen();
    glfwShowWindow(mCtx->window());

    // imgui must be initialized after input
    mInput = std::make_unique<glfw::Input>(mCtx->window());
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(mCtx->window(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

    mRenderSystem = std::make_unique<RenderSystem>(mCtx.get());

    mSettingsGui = std::make_unique<SettingsGui>();

    scene::Loader scene_loader{
        mCtx->allocator(), mCtx->device(), mCtx->physicalDevice(), mCtx->transferQueue, mCtx->mainQueue,
    };

    mScene = std::make_unique<scene::Scene>(std::move(scene_loader.load(SCENE_FILENAME)));
    mSunShadowCascade = std::make_unique<ShadowCascade>(
            mCtx->device(), mCtx->allocator(), mSettings.shadowCascade.resolution, Settings::SHADOW_CASCADE_COUNT
    );
    mBlobModel = std::make_unique<blob::Model>(mCtx->allocator(), mCtx->device(), BLOB_RESOLUTION);
    mSkybox = std::make_unique<Cubemap>(
            mCtx->allocator(), mCtx->device(), mCtx->transferQueue, mCtx->mainQueue, SKYBOX_FILENAMES
    );
    mCamera = std::make_unique<Camera>(FOV, NEAR_PLANE, CAMERA_POSITION, glm::vec3{});
    mDebugFrameTimes = std::make_unique<FrameTimes>();

    mAudio = std::make_unique<Audio>();
    mAmbientMusic = mAudio->createMusic("resources/audio/ambiance.ogg");
    mAmbientMusic->setLooping(true);
    mAmbientMusic->setVolume(0.05);

    mAnimationSampler = std::make_unique<scene::AnimationSampler>(mScene->cpu());

    mRenderSystem->recreate(mSettings);
}

Application::~Application() = default;

void Application::run() {
    mAmbientMusic->play();
    while (!mCtx->window().shouldClose()) {
        mRenderSystem->advance(mSettings);

        mInput->update();
        processInput();
        advanceAnimationTime();
        // if audio L/R is swapped then this should be (0,0,-1)
        mAudio->update(mCamera->position, mCamera->rotationMatrix() * glm::vec3(0, 0, 1));

        mRenderSystem->begin();
        mRenderSystem->imGuiBackend().beginFrame();

        drawGui();

        mCamera->setViewport(mCtx->swapchain().width(), mCtx->swapchain().height());
        updateSunShadowCascades();
        updateAnimatedInstances();

        mRenderSystem->draw({
            .gltfScene = mScene->gpu(),
            .camera = *mCamera,
            .sunShadowCasterCascade = *mSunShadowCascade,
            .sunLight = mSettings.sun,
            .settings = mSettings,
            .blobModel = *mBlobModel,
            .skybox = *mSkybox,
            .timestamp = mSettings.animation.time,
        });

        mRenderSystem->submit(mSettings);
    }

    mCtx->device().waitIdle();
}

void Application::processInput() {
    if (mInput->isKeyPress(GLFW_KEY_F5))
        reloadRenderSystem();

    updateMouseCapture();

    if (mInput->isMouseCaptured()) {
        updateCamera();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}

void Application::advanceAnimationTime() {
    if (!mSettings.animation.pause) {
        float dt = mInput->timeDelta() * mSettings.animation.playbackSpeed;
        mSettings.animation.time += dt;
    }
}

void Application::drawGui() {
    auto cpuRenderTimings = mRenderSystem->timings();
    mDebugFrameTimes->lines.emplace_back("Fence", static_cast<float>(cpuRenderTimings.fence));
    mDebugFrameTimes->lines.emplace_back("Advance", static_cast<float>(cpuRenderTimings.advance));
    mDebugFrameTimes->lines.emplace_back("Record", static_cast<float>(cpuRenderTimings.record));
    mDebugFrameTimes->lines.emplace_back("Submit", static_cast<float>(cpuRenderTimings.submit));
    mDebugFrameTimes->lines.emplace_back("Present", static_cast<float>(cpuRenderTimings.present));
    mDebugFrameTimes->lines.emplace_back("Total", static_cast<float>(cpuRenderTimings.total));
    mDebugFrameTimes->update(mInput->timeDelta());
    mDebugFrameTimes->draw();

    mSettingsGui->draw(mSettings);
}

void Application::updateSunShadowCascades() {
    mSunShadowCascade->lambda = mSettings.shadowCascade.lambda;
    mSunShadowCascade->distance = mSettings.shadowCascade.distance;
    mSunShadowCascade->update(mCamera->fov(), mCamera->aspect(), mCamera->viewMatrix(), -mSettings.sun.direction());

    for (size_t i = 0; i < mSettings.shadowCascades.size(); i++)
        mSettings.shadowCascades[i].applyTo(mSunShadowCascade->cascades()[i]);
}

void Application::updateAnimatedInstances() {
    std::vector<glm::mat4> animated_instance_transforms =
            mAnimationSampler->sampleAnimatedInstanceTransforms(mSettings.animation.time);

    if (!animated_instance_transforms.empty())
        mRenderSystem->updateInstanceTransforms(mScene->gpu(), animated_instance_transforms);
}

void Application::reloadRenderSystem() {
    Logger::info("Reloading render system");

    mCtx->device().waitIdle();

    try {
        mRenderSystem->recreate(mSettings);
    } catch (const std::exception &exc) {
        Logger::error("Reload failed: " + std::string(exc.what()));
    }
}

void Application::updateMouseCapture() {
    if (mInput->isMouseReleased() && mInput->isMousePress(GLFW_MOUSE_BUTTON_LEFT)) {
        if (!ImGui::GetIO().WantCaptureMouse)
            mInput->captureMouse();
    } else if (mInput->isMouseCaptured() && (mInput->isKeyPress(GLFW_KEY_ESCAPE) || mInput->isKeyPress(GLFW_KEY_LEFT_ALT)))
        mInput->releaseMouse();
}

void Application::updateCamera() {
    // Yaw
    mCamera->angles.y -= mInput->mouseDelta().x * MOUSE_SENSITIVITY;
    mCamera->angles.y = glm::wrapAngle(mCamera->angles.y);

    // Pitch
    mCamera->angles.x -= mInput->mouseDelta().y * MOUSE_SENSITIVITY;
    mCamera->angles.x = glm::clamp(mCamera->angles.x, -glm::half_pi<float>(), glm::half_pi<float>());

    glm::vec3 move_input = {
        mInput->isKeyDown(GLFW_KEY_D) - mInput->isKeyDown(GLFW_KEY_A),
        mInput->isKeyDown(GLFW_KEY_SPACE) - mInput->isKeyDown(GLFW_KEY_LEFT_CONTROL),
        mInput->isKeyDown(GLFW_KEY_S) - mInput->isKeyDown(GLFW_KEY_W)
    };

    glm::vec3 velocity = move_input * BASE_SPEED;
    velocity = glm::mat3(glm::rotate(glm::mat4(1.0f), mCamera->angles.y, {0, 1, 0})) * velocity;

    if (mInput->isKeyDown(GLFW_KEY_LEFT_SHIFT))
        velocity *= FAST_SPEED_MULTIPLIER;

    mCamera->position += velocity * mInput->timeDelta();
    mCamera->updateViewMatrix();
}
