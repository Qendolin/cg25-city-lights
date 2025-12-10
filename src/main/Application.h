#pragma once

#include <array>
#include <glm/glm.hpp>
#include <string>
#include <memory>

#include "debug/Settings.h"

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
    static constexpr char SCENE_FILENAME[] = "resources/scenes/CityTest.glb";
    static inline const std::array<std::string, 6> SKYBOX_FILENAMES{
        "resources/skybox/px.hdr", "resources/skybox/nx.hdr", "resources/skybox/py.hdr",
        "resources/skybox/ny.hdr", "resources/skybox/pz.hdr", "resources/skybox/nz.hdr",
    };

    // Order is important here
    std::unique_ptr<VulkanContext> ctx;
    std::unique_ptr<RenderSystem> render_system;

    Settings settings = {};
    std::unique_ptr<SettingsGui> settings_gui;

    std::unique_ptr<glfw::Input> input;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<scene::Scene> scene;
    std::unique_ptr<ShadowCascade> sun_shadow_cascade;

    std::unique_ptr<FrameTimes> debug_frame_times;

    std::unique_ptr<blob::Model> blob_model;
    std::unique_ptr<Cubemap> skybox;

public:
    Application();
    ~Application();

    void run();

private:
    void processInput();
    void drawGui();
    void updateSunShadowCascades();
    void reloadRenderSystem();
    void updateCamera();
    void updateMouseCapture();
};
