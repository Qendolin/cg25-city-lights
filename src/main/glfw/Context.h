#pragma once

#include <functional>
#include <vector>

namespace glfw {
    class Context {
        using ErrorCallback_T = std::function<void(int error, const char *description)>;

    public:
        Context() = delete;

        static void init(const ErrorCallback_T& error_callback = nullptr);

        static void terminate();

        static void setErrorCallback(const std::function<void(int error, const char *description)> &callback) {
            errorCallback = callback;
        }

        [[nodiscard]] static std::vector<const char *> getRequiredInstanceExtensions();

    private:
        static inline bool mInitialized = false;

        static inline ErrorCallback_T errorCallback = nullptr;

        static void defaultErrorCallback(int error, const char *description);
    };
} // namespace glfw
