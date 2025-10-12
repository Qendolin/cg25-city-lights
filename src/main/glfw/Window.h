#pragma once

#include <string>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;
struct GLFWmonitor;

namespace glfw {

    struct WindowCreateInfo {
        int width;
        int height;
        std::string title;
        bool resizable = true;
        bool visible = true;
        bool decorated = true;
        bool focused = true;
        bool autoIconify = true;
        bool floating = false;
        bool maximized = false;
        bool centerCursor = true;
        bool transparentFramebuffer = false;
        bool focusOnShow = true;
        bool scaleToMonitor = false;
        bool scaleFramebuffer = true;
        bool mousePassthrough = false;
        int positionX = static_cast<int>(0x80000000); // GLFW_ANY_POSITION
        int positionY = static_cast<int>(0x80000000); // GLFW_ANY_POSITION
        int redBits = 8;
        int greenBits = 8;
        int blueBits = 8;
        int alphaBits = 8;
        int depthBits = 24;
        int stencilBits = 8;
        int samples = 0;
        int refreshRate = -1;
        bool stereo = false;
        bool srgbCapable = false;
        bool doublebuffer = true;
    };

    class Window {
        GLFWwindow *mHandle = nullptr;

    public:

        Window(const WindowCreateInfo &create_info, GLFWmonitor *monitor = nullptr, GLFWwindow *share = nullptr);

        explicit Window(GLFWwindow *handle) : mHandle(handle) {}

        ~Window() {
            mHandle = nullptr;
        }

        Window() = default;

        Window(const Window &other) = default;

        Window &operator=(const Window &other) = default;

        [[nodiscard]] bool shouldClose() const;

        [[nodiscard]] vk::Extent2D getFramebufferSize() const;

        [[nodiscard]] vk::UniqueSurfaceKHR createWindowSurfaceKHRUnique(vk::Instance instance) const;

        operator GLFWwindow *() const { return mHandle; }

        void centerOnScreen() const;
    };



    using UniqueWindow = std::unique_ptr<Window>;
}


template<>
struct std::default_delete<glfw::Window>
{
    void operator()(glfw::Window *w) const;
};