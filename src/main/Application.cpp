#include "Application.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/gtc/type_ptr.inl>
#include <glm/gtx/fast_trigonometry.hpp>
#include <vulkan/vulkan.hpp>

#include "RenderSystem.h"
#include "SceneAnimation.h"
#include "audio/Audio.h"
#include "backend/Swapchain.h"
#include "backend/VulkanContext.h"
#include "blob/HenonHeiles.h"
#include "blob/System.h"
#include "debug/Performance.h"
#include "debug/SettingsGui.h"
#include "entity/Camera.h"
#include "entity/Cubemap.h"
#include "entity/ShadowCaster.h"
#include "glfw/Input.h"
#include "imgui/ImGui.h"
#include "scene/EnvironmentLighting.h"
#include "scene/Gltf.h"
#include "scene/Loader.h"
#include "scene/Scene.h"
#include "util/Color.h"
#include "util/Logger.h"

Application::Application() {
    mSettings.camera.debugCamera = globals::Debug;
    mSettings.showGui = globals::Debug;
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

    if (!globals::Debug) {
        auto monitor = glfwGetPrimaryMonitor();
        int x, y, w, h;
        glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
        glfwSetWindowMonitor(mCtx->window(), monitor, x, y, w, h, GLFW_DONT_CARE);
    }
    glfwShowWindow(mCtx->window());
    glfwFocusWindow(mCtx->window());
    mInput->captureMouse();
}

Application::~Application() = default;

void Application::run() {
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
        updateGpuData();

        mRenderSystem->draw({
            .gltfScene = mScene->gpu(),
            .camera = activeCamera(),
            .sunShadowCasterCascade = *mSunShadowCascade,
            .sunLight = mSettings.sun,
            .settings = mSettings,
            .blobSystem = *mBlobSystem,
            .skyboxDay = *mSkyboxDay,
            .skyboxNight = *mSkyboxNight,
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

    // ReSharper disable once CppDeprecatedEntity
    const char *env_scene = std::getenv("SCENE");
    std::string scene_filename = env_scene ? env_scene : "";

    if (scene_filename.empty())
        scene_filename = DEFAULT_SCENE_FILENAME;

    Logger::info("Loading scene from file: " + scene_filename);
    mScene = std::make_unique<scene::Scene>(std::move(scene_loader.load(scene_filename)));
    mSunShadowCascade = std::make_unique<ShadowCascade>(
            mCtx->device(), mCtx->allocator(), mSettings.shadowCascade.resolution, Settings::SHADOW_CASCADE_COUNT
    );
    mSkyboxDay = std::make_unique<Cubemap>(
            mCtx->allocator(), mCtx->device(), mCtx->transferQueue, mCtx->mainQueue,
            Cubemap::makeSkyboxImageFilenames(SKYBOX_DAY)
    );
    mSkyboxNight = std::make_unique<Cubemap>(
            mCtx->allocator(), mCtx->device(), mCtx->transferQueue, mCtx->mainQueue,
            Cubemap::makeSkyboxImageFilenames(SKYBOX_NIGHT)
    );

    mBlobSystem = std::make_unique<blob::System>(mCtx->allocator(), mCtx->device(), 6, BLOB_RESOLUTION);
    mBlobChaos = std::make_unique<HenonHeiles>(6);
}

void Application::initCameras() {
    mDebugCamera = std::make_unique<Camera>(FOV, NEAR_PLANE, DEFAULT_CAMERA_POSITION, glm::vec3{});

    if (mScene->cpu().non_mesh_instance_animation_map.contains("Camera")) {
        auto [cam_instance_idx, cam_anim_idx] = mScene->cpu().non_mesh_instance_animation_map.at("Camera");
        const glm::mat4 cam_instance_transform = mScene->cpu().instances[cam_instance_idx].transform;
        mAnimatedCamera = std::make_unique<Camera>(FOV, NEAR_PLANE, cam_instance_transform);
    } else {
        mAnimatedCamera = std::make_unique<Camera>(FOV, NEAR_PLANE, DEFAULT_CAMERA_POSITION, glm::vec3{});
    }
}

void Application::initAudio() {
    mAudio = std::make_unique<Audio>();
    mSceneAudio.ambientMusic = mAudio->createMusic("resources/audio/ambiance.ogg");
    mSceneAudio.ambientMusic->setLooping(true);

    mSceneAudio.engineSoundBus = mAudio->createSound("resources/audio/chugging-diesel-bus-and-rev-23478.ogg");
    mSceneAudio.engineSoundAlt = mAudio->createSound("resources/audio/engine-47745.ogg");
    mSceneAudio.engineSoundAlt->setLooping(true);

    mSceneAudio.engineSoundInstanceBlueCar = std::unique_ptr<SoundInstance3d>(mSceneAudio.engineSoundAlt->play3d(glm::vec3{}, 0.0f));
    mSceneAudio.engineSoundInstanceBlueVan = std::unique_ptr<SoundInstance3d>(mSceneAudio.engineSoundBus->play3d(glm::vec3{}, 0.0f));
    mSceneAudio.engineSoundInstanceBlueVan->pause();
    mSceneAudio.engineSoundInstanceWhiteVan = std::unique_ptr<SoundInstance3d>(mSceneAudio.engineSoundBus->play3d(glm::vec3{}, 0.0f));
    mSceneAudio.engineSoundInstanceWhiteVan->pause();

    mSceneAudio.ufoSound = mAudio->createSound("resources/audio/spaceship-hum-low-frequency-296518.ogg");
    mSceneAudio.ufoSound->setLooping(true);
    mSceneAudio.ufoSoundInstance = std::unique_ptr<SoundInstance3d>(mSceneAudio.ufoSound->play3d(glm::vec3{}, 0.0f));

    mSceneAudio.lidShutSound = mAudio->createSound("resources/audio/car-trunk-closing-421362.wav");
    mSceneAudio.dumpsterOpenSound = mAudio->createSound("resources/audio/046422_trash-can-falling-over-71483.ogg");

    mSceneAudio.beamSound = mAudio->createSound("resources/audio/scifi-sound-85501.ogg");
    mSceneAudio.beamSound->setLooping(true);
    mSceneAudio.beamSoundInstance = std::unique_ptr<SoundInstance3d>(mSceneAudio.beamSound->play3d(glm::vec3{}, 0.0f));
}

void Application::initVariableAnimations() {
    mTimeline = std::make_unique<Timeline>();
    createSceneAnimation(*mTimeline, mSettings, *mScene, mSceneAudio);
}

void Application::processInput() {
    if (mInput->isKeyPress(GLFW_KEY_F5))
        reloadRenderSystem();

    if (mInput->isKeyPress(GLFW_KEY_F1))
        mSettings.showGui = !mSettings.showGui;

    if (mInput->isKeyPress(GLFW_KEY_F11)) {
        if (glfwGetWindowMonitor(mCtx->window())) {
            glfwSetWindowMonitor(mCtx->window(), nullptr, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GLFW_DONT_CARE);
            mCtx->window().centerOnScreen();
        } else {
            auto monitor = glfwGetPrimaryMonitor();
            int x, y, w, h;
            glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);
            glfwSetWindowMonitor(mCtx->window(), monitor, x, y, w, h, GLFW_DONT_CARE);
        }
    }

    if (mInput->isKeyPress(GLFW_KEY_P)) {
        mSettings.animation.time = 0;
        mSettings.animation.pause = false;
        mSettings.camera.debugCamera = false;
        mTimeline->reset();
    }

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
    const glm::mat4 anim_cam_transform = mInstanceAnimationSampler->sampleNamedTransform("Camera", mSettings.animation.time);
    mAnimatedCamera->updateBasedOnTransform(anim_cam_transform);
}

void Application::updateBlob() {
    if (!mSettings.animation.animateBlobNode)
        return;

    // NOTE: Ground is currently disabled in the shader
    mBlobSystem->groundLevel = 0.1f;
    auto balls = mBlobSystem->balls();

    float time = mSettings.animation.time;
    glm::vec3 center = mInstanceAnimationSampler->sampleNamedTranslation("Blob", time);

    mBlobChaos->update(std::min(mInput->timeDelta() * mSettings.blob.animationSpeed, 1.0f / 30.0f));
    for (size_t i = 0; i < balls.size(); i++) {
        auto &ball = balls[i];
        ball.baseRadius = mSettings.blob.baseRadius;
        ball.maxRadius = mSettings.blob.maxRadius;

        glm::vec3 vec = mBlobChaos->points[i].position;
        vec = glm::normalize(vec) * std::pow(glm::length(vec), mSettings.blob.dispersionPower);
        ball.center = center + vec * glm::vec3{mSettings.blob.dispersionXZ, mSettings.blob.dispersionY, mSettings.blob.dispersionXZ};
    }
}

void Application::updateAudio() {
    const Camera &camera = activeCamera();

    mAudio->system->setVolume(mSettings.audio.masterVolume);
    mSceneAudio.ambientMusic->setVolume(mSettings.audio.ambientVolume);

    glm::vec3 blue_van_pos = mInstanceAnimationSampler->sampleNamedTranslation("Blue Van.Sound", mSettings.animation.time);
    mSceneAudio.engineSoundInstanceBlueVan->setPosition(blue_van_pos);
    mSceneAudio.engineSoundInstanceBlueVan->setVolume(mSettings.audio.blueVanVolume);
    glm::vec3 blue_car_pos = mInstanceAnimationSampler->sampleNamedTranslation("Blue Car.Sound", mSettings.animation.time);
    mSceneAudio.engineSoundInstanceBlueCar->setPosition(blue_car_pos);
    mSceneAudio.engineSoundInstanceBlueCar->setVolume(mSettings.audio.blueCarVolume);
    glm::vec3 white_van_pos = mInstanceAnimationSampler->sampleNamedTranslation("White Van.Sound", mSettings.animation.time);
    mSceneAudio.engineSoundInstanceWhiteVan->setPosition(white_van_pos);
    mSceneAudio.engineSoundInstanceWhiteVan->setVolume(mSettings.audio.whiteVanVolume);

    glm::vec3 ufo_pos = mInstanceAnimationSampler->sampleNamedTranslation("UFO.Sound", mSettings.animation.time);
    mSceneAudio.ufoSoundInstance->setPosition(ufo_pos);
    mSceneAudio.ufoSoundInstance->setVolume(mSettings.audio.ufoVolume);

    mSceneAudio.beamSoundInstance->setPosition(ufo_pos);

    mAudio->update(camera.position, camera.rotationMatrix() * glm::vec3(0, 0, -1));
}

void Application::updateAnimatedVariables() {
    if (mSettings.animation.animateVariables)
        mVariableAnimationController.update(mSettings.animation.time);

    mTimeline->update(static_cast<uint32_t>(mSettings.animation.time * 1000));

    auto radiance = lighting::sunLightFromElevation(mSettings.sun.elevation);
    mSettings.sun.color = radiance;

    auto ambient_radiance = lighting::ambientSkyLightFromElevation(mSettings.sun.elevation);
    mSettings.rendering.ambient = ambient_radiance;

    mSettings.sky.dayNightBlend = 1.0f - glm::smoothstep(-18.0f, 0.0f, mSettings.sun.elevation);
    mSettings.sky.exposure = -2.25f + (2.25f + 1.5f) * glm::smoothstep(-18.0f, 0.0f, mSettings.sun.elevation);

    if (mSettings.animation.animateLights) {
        for (size_t i = 0; i < scene::Loader::DYNAMIC_LIGHTS_RESERVATION; i++) {
            size_t offset = mScene->cpu().lights.size() - scene::Loader::DYNAMIC_LIGHTS_RESERVATION;
            auto& light = mScene->cpu().lights[offset + i];
            light.position.y += (std::max(light.position.y, 0.0f) * 0.5f + 3.0f) * mInput->timeDelta();
            light.position.y = std::fmodf(light.position.y, 40.0f);
        }
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

void Application::updateGpuData() {
    std::vector<glm::mat4> animated_instance_transforms =
            mInstanceAnimationSampler->sampleAnimatedInstanceTransforms(mSettings.animation.time);

    if (!animated_instance_transforms.empty())
        mRenderSystem->updateInstanceTransforms(mScene->gpu(), animated_instance_transforms);

    mRenderSystem->updateLights(mScene->gpu(), mScene->cpu().lights);
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
