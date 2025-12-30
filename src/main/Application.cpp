#include "Application.h"

#include <GLFW/glfw3.h>
#include <algorithm>
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
#include "scene/Gltf.h"
#include "scene/Loader.h"
#include "scene/Scene.h"
#include "util/Logger.h"

Application::Application() {
    initContext();
    initInput();
    // imgui must be initialized after input
    mRenderSystem = std::make_unique<RenderSystem>(mCtx.get());
    mSettingsGui = std::make_unique<SettingsGui>();
    initScene();
    initCameras();
    initAudio();
    initVariableAnimations();
    mDebugFrameTimes = std::make_unique<FrameTimes>();
    mInstanceAnimationSampler = std::make_unique<InstanceAnimationSampler>(mScene->cpu());
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
        updateDebugCamera();
        updateAnimatedCamera();
        updateBlob();
        updateAudio();
        updateAnimatedVariables();

        mRenderSystem->begin();
        mRenderSystem->imGuiBackend().beginFrame();

        drawGui();

        updateViewport();
        updateSunShadowCascades();
        updateAnimatedInstances();

        mRenderSystem->draw({
            .gltfScene = mScene->gpu(),
            .camera = activeCamera(),
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

void Application::initContext() {
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
}

void Application::initInput() {
    mInput = std::make_unique<glfw::Input>(mCtx->window());

    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(mCtx->window(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
}

void Application::initScene() {
    scene::Loader scene_loader{
        mCtx->allocator(), mCtx->device(), mCtx->physicalDevice(), mCtx->transferQueue, mCtx->mainQueue,
    };

    const char *env_scene = std::getenv("SCENE");
    std::string scene_filename = env_scene ? env_scene : "";

    if (scene_filename.empty())
        scene_filename = DEFAULT_SCENE_FILENAME;

    Logger::info("Loading scene from file: " + scene_filename);
    mScene = std::make_unique<scene::Scene>(std::move(scene_loader.load(scene_filename)));
    mSunShadowCascade = std::make_unique<ShadowCascade>(
            mCtx->device(), mCtx->allocator(), mSettings.shadowCascade.resolution, Settings::SHADOW_CASCADE_COUNT
    );
    mSkybox = std::make_unique<Cubemap>(
            mCtx->allocator(), mCtx->device(), mCtx->transferQueue, mCtx->mainQueue, SKYBOX_FILENAMES
    );

    glm::mat4 blob_instance_transform;

    if (mScene->cpu().animated_blob_exists) {
        const std::size_t blob_instance_idx = mScene->cpu().animated_blob_index;
        blob_instance_transform = mScene->cpu().instances[blob_instance_idx].transform;
    } else
        blob_instance_transform = glm::translate(glm::mat4(1.0f), DEFAULT_BLOB_POSITION);

    mBlobModel = std::make_unique<blob::Model>(mCtx->allocator(), mCtx->device(), BLOB_RESOLUTION, blob_instance_transform);
}

void Application::initCameras() {
    mDebugCamera = std::make_unique<Camera>(FOV, NEAR_PLANE, DEFAULT_CAMERA_POSITION, glm::vec3{});

    if (mScene->cpu().animated_camera_exists) {
        const std::size_t cam_instance_idx = mScene->cpu().animated_camera_index;
        const glm::mat4 cam_instance_transform = mScene->cpu().instances[cam_instance_idx].transform;
        mAnimatedCamera = std::make_unique<Camera>(FOV, NEAR_PLANE, cam_instance_transform);
    } else {
        mAnimatedCamera = std::make_unique<Camera>(FOV, NEAR_PLANE, DEFAULT_CAMERA_POSITION, glm::vec3{});
    }
}

void Application::initAudio() {
    mAudio = std::make_unique<Audio>();
    mAmbientMusic = mAudio->createMusic(AMBIENT_SOUND_FILENAME);
    mAmbientMusic->setLooping(true);
    mAmbientMusic->setVolume(0.05);
}

void Application::initVariableAnimations() {
    mVariableAnimationController.createTrack(mSettings.sky.exposure);
    mVariableAnimationController.track(mSettings.sky.exposure).addKeyframe(0.f, 1.49f);
    mVariableAnimationController.track(mSettings.sky.exposure).addKeyframe(12.f, 1.49f);
    mVariableAnimationController.track(mSettings.sky.exposure).addKeyframe(14.f, 0.f);
    mVariableAnimationController.track(mSettings.sky.exposure).addKeyframe(15.5f, -4.5f);

    mVariableAnimationController.createTrack(mSettings.sun.color);
    mVariableAnimationController.track(mSettings.sun.color).addKeyframe(0.f, glm::vec3{1.f});
    mVariableAnimationController.track(mSettings.sun.color).addKeyframe(12.f, glm::vec3{1.f});
    mVariableAnimationController.track(mSettings.sun.color).addKeyframe(13.5f, glm::vec3{1.f});
    mVariableAnimationController.track(mSettings.sun.color).addKeyframe(15.f, glm::vec3{1.f, 0.6f, 0.6f});

    mVariableAnimationController.createTrack(mSettings.sun.power);
    mVariableAnimationController.track(mSettings.sun.power).addKeyframe(0.f, 15.0f);
    mVariableAnimationController.track(mSettings.sun.power).addKeyframe(12.f, 15.0f);
    mVariableAnimationController.track(mSettings.sun.power).addKeyframe(15.f, 10.0f);
    mVariableAnimationController.track(mSettings.sun.power).addKeyframe(16.f, 0.0f);

    mVariableAnimationController.createTrack(mSettings.sun.elevation);
    mVariableAnimationController.track(mSettings.sun.elevation).addKeyframe(0.f, 40.f);
    mVariableAnimationController.track(mSettings.sun.elevation).addKeyframe(12.f, 40.f);
    mVariableAnimationController.track(mSettings.sun.elevation).addKeyframe(18.f, 0.0f);

    mVariableAnimationController.createTrack(mSettings.rendering.ambient);
    mVariableAnimationController.track(mSettings.rendering.ambient).addKeyframe(0.f, glm::vec3{0.28f, 0.315, 0.385});
    mVariableAnimationController.track(mSettings.rendering.ambient).addKeyframe(12.f, glm::vec3{0.28f, 0.315, 0.385});
    mVariableAnimationController.track(mSettings.rendering.ambient).addKeyframe(15.f, glm::vec3{0.056f, 0.063, 0.077});
    mVariableAnimationController.track(mSettings.rendering.ambient).addKeyframe(16.f, glm::vec3{0.028f, 0.032, 0.039});
}

void Application::processInput() {
    if (mInput->isKeyPress(GLFW_KEY_F5))
        reloadRenderSystem();

    updateMouseCapture();

    if (mInput->isMouseCaptured()) {
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

void Application::updateAnimatedCamera() {
    if (!mScene->cpu().animated_camera_exists)
        return;

    const glm::mat4 anim_cam_transform = mInstanceAnimationSampler->sampleAnimatedCameraTransform(mSettings.animation.time);
    mAnimatedCamera->updateBasedOnTransform(anim_cam_transform);
}

void Application::updateBlob() {
    if (!mScene->cpu().animated_blob_exists || !mSettings.animation.animateBlobNode)
        return;

    const glm::mat4 anim_blob_transform = mInstanceAnimationSampler->sampleAnimatedBlobTransform(mSettings.animation.time);
    mBlobModel->setTransform(anim_blob_transform);

    const float translation_y = anim_blob_transform[3][1];

    // Keep the ground level (for drop effects) at y = 0 in world space
    mBlobModel->groundLevel = -translation_y;

    // Make the blob smaller when it's closer to the ground, so that it doesn't clip
    mBlobModel->size = std::min(std::max(0.f, translation_y), 1.f);
}

void Application::updateAudio() {
    const Camera &camera = activeCamera();
    // if audio L/R is swapped then this should be (0,0,-1)
    mAudio->update(camera.position, camera.rotationMatrix() * glm::vec3(0, 0, 1));
}

void Application::updateAnimatedVariables() {
    if (mSettings.animation.animateVariables)
        mVariableAnimationController.update(mSettings.animation.time);
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

void Application::updateViewport() {
    const float width = mCtx->swapchain().width();
    const float height = mCtx->swapchain().height();

    mDebugCamera->setViewport(width, height);
    mAnimatedCamera->setViewport(width, height);
}

void Application::updateSunShadowCascades() {
    const Camera &camera = activeCamera();

    mSunShadowCascade->lambda = mSettings.shadowCascade.lambda;
    mSunShadowCascade->distance = mSettings.shadowCascade.distance;
    mSunShadowCascade->update(camera.fov(), camera.aspect(), camera.viewMatrix(), -mSettings.sun.direction());

    for (size_t i = 0; i < mSettings.shadowCascades.size(); i++)
        mSettings.shadowCascades[i].applyTo(mSunShadowCascade->cascades()[i]);
}

void Application::updateAnimatedInstances() {
    std::vector<glm::mat4> animated_instance_transforms =
            mInstanceAnimationSampler->sampleAnimatedInstanceTransforms(mSettings.animation.time);

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

void Application::updateDebugCamera() {
    if (!(mSettings.camera.debugCamera && mInput->isMouseCaptured()))
        return;

    // Yaw
    mDebugCamera->angles.y -= mInput->mouseDelta().x * MOUSE_SENSITIVITY;
    mDebugCamera->angles.y = glm::wrapAngle(mDebugCamera->angles.y);

    // Pitch
    mDebugCamera->angles.x -= mInput->mouseDelta().y * MOUSE_SENSITIVITY;
    mDebugCamera->angles.x = glm::clamp(mDebugCamera->angles.x, -glm::half_pi<float>(), glm::half_pi<float>());

    glm::vec3 move_input = {
        mInput->isKeyDown(GLFW_KEY_D) - mInput->isKeyDown(GLFW_KEY_A),
        mInput->isKeyDown(GLFW_KEY_SPACE) - mInput->isKeyDown(GLFW_KEY_LEFT_CONTROL),
        mInput->isKeyDown(GLFW_KEY_S) - mInput->isKeyDown(GLFW_KEY_W)
    };

    glm::vec3 velocity = move_input * BASE_SPEED;
    velocity = glm::mat3(glm::rotate(glm::mat4(1.0f), mDebugCamera->angles.y, {0, 1, 0})) * velocity;

    if (mInput->isKeyDown(GLFW_KEY_LEFT_SHIFT))
        velocity *= FAST_SPEED_MULTIPLIER;

    mDebugCamera->position += velocity * mInput->timeDelta();
    mDebugCamera->updateViewMatrix();
}

const Camera &Application::activeCamera() const {
    return (mSettings.camera.debugCamera ? *mDebugCamera : *mAnimatedCamera);
}
