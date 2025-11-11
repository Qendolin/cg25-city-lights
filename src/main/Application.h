#pragma once

#include <vulkan/vulkan.hpp>

#include "backend/Descriptors.h"
#include "backend/Framebuffer.h"
#include "debug/Settings.h"
#include "util/PerFrame.h"

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

// TODO:
// Why do we not forward declare by including the header files here instead of in the cpp file?
namespace blob {
    class BlobSdf;
    class Mesher;
    class Model;
};

struct BlobMesherConfig {
    float intervalStart;
    float intervalEnd;
    int resolution;
    float isoValue;
};

class Application {
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

    static constexpr BlobMesherConfig BLOB_MESHER_CONFIG{-1, 1, 24, 0};
    std::unique_ptr<blob::BlobSdf> blobSdf;
    std::unique_ptr<blob::Mesher> mcMesher;

    std::unique_ptr<blob::Model> blobModel;

    void processInput();
    void drawGui();

public:
    Application();
    ~Application();

    void init();
    void run();
};
