#pragma once

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

#include "debug/Settings.h"
#include "scene/AnimationSampler.h"

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
    class Scene;
}
namespace blob {
    class Model;
};
class Cubemap;

class Application {
private:
    static constexpr int BLOB_RESOLUTION = 50;
    static constexpr float MOUSE_SENSITIVITY = 0.0026;
    static constexpr float BASE_SPEED = 5.0f;
    static constexpr float FAST_SPEED_MULTIPLIER = 10.0f;
    static constexpr int WINDOW_WIDTH = 1600;
    static constexpr int WINDOW_HEIGHT = 900;
    static constexpr float FOV = glm::radians(90.0f);
    static constexpr float NEAR_PLANE = 0.001f;
    static constexpr glm::vec3 CAMERA_POSITION = glm::vec3{0, 1, 5};
    static constexpr char TITLE[] = "City Lights";
    static constexpr char SCENE_FILENAME[] = "resources/scenes/city_animated.glb";
    static inline const std::array<std::string, 6> SKYBOX_FILENAMES{
        "resources/skybox/px.hdr", "resources/skybox/nx.hdr", "resources/skybox/py.hdr",
        "resources/skybox/ny.hdr", "resources/skybox/pz.hdr", "resources/skybox/nz.hdr",
    };

    // Order is important here
    std::unique_ptr<VulkanContext> mCtx;
    std::unique_ptr<RenderSystem> mRenderSystem;

    Settings mSettings = {};
    std::unique_ptr<SettingsGui> mSettingsGui;

    std::unique_ptr<glfw::Input> mInput;
    std::unique_ptr<Camera> mCamera;
    std::unique_ptr<scene::Scene> mScene;
    std::unique_ptr<ShadowCascade> mSunShadowCascade;

    std::unique_ptr<FrameTimes> mDebugFrameTimes;

    std::unique_ptr<blob::Model> mBlobModel;
    std::unique_ptr<Cubemap> mSkybox;

    std::vector<scene::InstanceAnimationCursor> mAnimationCursorCache;

public:
    Application();
    ~Application();

    void run();

private:
    void processInput();
    void drawGui();
    void updateSunShadowCascades();
    void updateAnimatedInstances();
    void reloadRenderSystem();
    void updateCamera();
    void updateMouseCapture();
};
