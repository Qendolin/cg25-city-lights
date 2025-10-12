#pragma once

#include <functional>
#include <vector>

namespace glfw {
    /// <summary>
    /// Manages the GLFW library context.
    /// </summary>
    class Context {
        using ErrorCallback_T = std::function<void(int error, const char *description)>;

    public:
        Context() = delete;

        /// <summary>
        /// Initializes the GLFW library.
        /// </summary>
        /// <param name="error_callback">An optional callback function for handling GLFW errors.</param>
        static void init(const ErrorCallback_T& error_callback = nullptr);

        /// <summary>
        /// Terminates the GLFW library, cleaning up its resources.
        /// </summary>
        static void terminate();

        /// <summary>
        /// Sets the GLFW error callback.
        /// </summary>
        /// <param name="callback">The function to be called when a GLFW error occurs.</param>
        static void setErrorCallback(const std::function<void(int error, const char *description)> &callback) {
            errorCallback = callback;
        }

        /// <summary>
        /// Retrieves the required Vulkan instance extensions.
        /// </summary>
        /// <returns>A vector of strings containing the names of the required instance extensions.</returns>
        [[nodiscard]] static std::vector<const char *> getRequiredInstanceExtensions();

    private:
        static inline bool mInitialized = false;

        static inline ErrorCallback_T errorCallback = nullptr;

        static void defaultErrorCallback(int error, const char *description);
    };
} // namespace glfw
