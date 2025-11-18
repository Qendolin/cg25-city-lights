#pragma once

#include <vulkan/vulkan.hpp>

#include "debug/Settings.h"

class RenderSystem;
class FinalizeRenderer;
class ShadowCaster;
class ShadowRenderer;
class SettingsGui;
struct FrameTimes;
namespace glfw {
    class Input;
}
class Camera;
class VulkanContext;
class Framebuffer;
class PbrSceneRenderer;
namespace scene {
    class Scene;
}
class ImGuiBackend;
class ShaderLoader;

namespace blob {
    class Model;
};

struct BlobMesherConfig {
    float intervalStart;
    float intervalEnd;
    int resolution;
    float isoValue;
};

class Application {
    static constexpr int BLOB_RESOLUTION = 50;

    // Order is important here
    std::unique_ptr<VulkanContext> context;
    std::unique_ptr<RenderSystem> renderSystem;

    std::unique_ptr<glfw::Input> input;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<scene::Scene> scene;
    std::unique_ptr<ShadowCaster> sunShadowCaster;

    Settings settings = {};
    std::unique_ptr<SettingsGui> settingsGui;

    std::unique_ptr<FrameTimes> debugFrameTimes;

    std::unique_ptr<blob::Model> blobModel;

    void processInput();
    void drawGui();

public:
    Application();
    ~Application();

    void init();
    void run();
};
