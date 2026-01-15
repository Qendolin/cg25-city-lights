#pragma once

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <string>

#include "animation/InstanceAnimationSampler.h"
#include "animation/VariableAnimationController.h"
#include "debug/Settings.h"


class HenonHeiles;
namespace blob {
    class System;
}
class Sound;
class Audio;
class Music;
class ShadowCascade;
class RenderSystem;
class SettingsGui;
struct FrameTimes;
namespace glfw {
    class Input;
}
class Camera;
class VulkanContext;
namespace scene {
    class AnimationSampler;
    class Scene;
}
namespace blob {
    class Model;
};
class Cubemap;

class Application {
private:
    static constexpr float BLOB_RESOLUTION{1.0/20.0};
    static constexpr float MOUSE_SENSITIVITY{0.0026f};
    static constexpr float BASE_SPEED{5.0f};
    static constexpr float FAST_SPEED_MULTIPLIER{10.0f};
    static constexpr int WINDOW_WIDTH{1600};
    static constexpr int WINDOW_HEIGHT{900};
    static constexpr float FOV{glm::radians(90.0f)};
    static constexpr float NEAR_PLANE{0.001f};
    static constexpr glm::vec3 DEFAULT_CAMERA_POSITION{0.0f, 1.0f, 5.0f};
    static constexpr glm::vec3 DEFAULT_BLOB_POSITION{0.0f, 1.0f, 0.0f};
    static constexpr char TITLE[]{"City Lights"};
    static constexpr char DEFAULT_SCENE_FILENAME[]{"resources/scenes/CityTest.glb"};
    static constexpr char AMBIENT_SOUND_FILENAME[]{"resources/audio/ambiance.ogg"};
    static constexpr char  SKYBOX_DAY[] = "resources/skybox/evening_road_01_puresky_2k";
    static constexpr char  SKYBOX_NIGHT[] = "resources/skybox/NightSkyHDRI002_2K_HDR";

    // Order is important here
    std::unique_ptr<VulkanContext> mCtx;
    std::unique_ptr<RenderSystem> mRenderSystem;

    Settings mSettings = {};
    std::unique_ptr<SettingsGui> mSettingsGui;

    std::unique_ptr<glfw::Input> mInput;
    std::unique_ptr<Camera> mDebugCamera;
    std::unique_ptr<Camera> mAnimatedCamera;
    std::unique_ptr<scene::Scene> mScene;
    std::unique_ptr<ShadowCascade> mSunShadowCascade;

    std::unique_ptr<FrameTimes> mDebugFrameTimes;

    std::unique_ptr<blob::System> mBlobSystem;
    std::unique_ptr<HenonHeiles> mBlobChaos;
    std::unique_ptr<Cubemap> mSkyboxDay;
    std::unique_ptr<Cubemap> mSkyboxNight;

    std::unique_ptr<Audio> mAudio;
    std::unique_ptr<Music> mAmbientMusic;

    std::unique_ptr<InstanceAnimationSampler> mInstanceAnimationSampler;
    VariableAnimationController mVariableAnimationController{};

public:
    Application();
    ~Application();

    void run();

private:
    void initContext();
    void initInput();
    void initScene();
    void initCameras();
    void initAudio();
    void initVariableAnimations();

    void processInput();
    void advanceAnimationTime();
    void updateAnimatedCamera();
    void updateBlob();
    void updateAudio();
    void updateAnimatedVariables();
    void drawGui();
    void updateViewport();
    void updateSunShadowCascades();
    void updateGpuData();
    void reloadRenderSystem();
    void updateDebugCamera();
    void updateMouseCapture();

    const Camera &activeCamera() const;
};
