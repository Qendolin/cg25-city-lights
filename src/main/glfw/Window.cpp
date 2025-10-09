#include "Window.h"

#include <GLFW/glfw3.h>

namespace glfw {
    Window::Window(const WindowCreateInfo &create_info, GLFWmonitor *monitor, GLFWwindow *share) {
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_RESIZABLE, create_info.resizable);
        glfwWindowHint(GLFW_VISIBLE, create_info.visible);
        glfwWindowHint(GLFW_DECORATED, create_info.decorated);
        glfwWindowHint(GLFW_FOCUSED, create_info.focused);
        glfwWindowHint(GLFW_AUTO_ICONIFY, create_info.autoIconify);
        glfwWindowHint(GLFW_FLOATING, create_info.floating);
        glfwWindowHint(GLFW_MAXIMIZED, create_info.maximized);
        glfwWindowHint(GLFW_CENTER_CURSOR, create_info.centerCursor);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, create_info.transparentFramebuffer);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, create_info.focusOnShow);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, create_info.scaleToMonitor);
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, create_info.scaleFramebuffer);
        glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, create_info.mousePassthrough);
        glfwWindowHint(GLFW_POSITION_X, create_info.positionX);
        glfwWindowHint(GLFW_POSITION_Y, create_info.positionY);
        glfwWindowHint(GLFW_RED_BITS, create_info.redBits);
        glfwWindowHint(GLFW_GREEN_BITS, create_info.greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, create_info.blueBits);
        glfwWindowHint(GLFW_ALPHA_BITS, create_info.alphaBits);
        glfwWindowHint(GLFW_DEPTH_BITS, create_info.depthBits);
        glfwWindowHint(GLFW_STENCIL_BITS, create_info.stencilBits);
        glfwWindowHint(GLFW_SAMPLES, create_info.samples);
        glfwWindowHint(GLFW_REFRESH_RATE, create_info.refreshRate);
        glfwWindowHint(GLFW_STEREO, create_info.stereo);
        glfwWindowHint(GLFW_SRGB_CAPABLE, create_info.srgbCapable);
        glfwWindowHint(GLFW_DOUBLEBUFFER, create_info.doublebuffer);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        mHandle = glfwCreateWindow(create_info.width, create_info.height, create_info.title.c_str(), monitor, share);
    }

    bool Window::shouldClose() const { return glfwWindowShouldClose(mHandle); }

    vk::Extent2D Window::getFramebufferSize() const {
        int width, height;
        glfwGetFramebufferSize(mHandle, &width, &height);
        return vk::Extent2D(width, height);
    }

    vk::UniqueSurfaceKHR Window::createWindowSurfaceKHRUnique(vk::Instance instance) const {
        VkSurfaceKHR surface_handle;
        VkResult result = glfwCreateWindowSurface(instance, mHandle, nullptr, &surface_handle);
        vk::detail::resultCheck(static_cast<vk::Result>(result), "glfwCreateWindowSurface");
        return vk::UniqueSurfaceKHR(surface_handle, instance);
    }

    void Window::centerOnScreen() const {
        auto monitor = glfwGetWindowMonitor(mHandle);
        if (!monitor) {
            monitor = glfwGetPrimaryMonitor();
        }
        int x, y, mw, mh;
        glfwGetMonitorWorkarea(monitor, &x, &y, &mw, &mh);
        int ww, wh;
        glfwGetWindowSize(mHandle, &ww, &wh);
        glfwSetWindowPos(mHandle, x + mw / 2 - ww / 2, y + mh / 2 - wh / 2);
    }
}

void std::default_delete<glfw::Window>::operator()(glfw::Window *ptr) const {
    if (static_cast<GLFWwindow *>(*ptr))
        glfwDestroyWindow(*ptr);
    delete ptr;
}
