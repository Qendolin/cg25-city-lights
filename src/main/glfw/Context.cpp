#include "Context.h"

#include <GLFW/glfw3.h>
#include <iostream>

namespace glfw {
    void Context::defaultErrorCallback(int error, const char *description) {
        if (errorCallback) {
            errorCallback(error, description);
        } else {
            std::cerr << "GLFW error " << std::format("{:#010x}", error) << ": " << description << std::endl;
        }
    }

    void Context::init(const ErrorCallback_T& error_callback) {
        if (mInitialized) {
            throw std::runtime_error("GLFW is already initialized");
        }
        errorCallback = error_callback;
        glfwSetErrorCallback(defaultErrorCallback);
        if (!glfwInit()) {
            throw std::runtime_error("GLFW initialization failed");
        }
        mInitialized = true;

        if (!glfwVulkanSupported()) {
            throw std::runtime_error("GLFW vulkan not supported");
        }
    }

    void Context::terminate() {
        glfwTerminate();
        mInitialized = false;
    }

    std::vector<const char *> Context::getRequiredInstanceExtensions() {
        uint32_t count;
        const char **extensions = glfwGetRequiredInstanceExtensions(&count);
        return {extensions, extensions + count};
    }
}
