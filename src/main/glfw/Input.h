#pragma once

#include <array>
#include <functional>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace glfw {
    class Window;
}

struct GLFWwindow;

namespace glfw {
    /// <summary>
    /// The input class handles user input.
    /// There are quite a few considerations to be made:
    /// <list type="bullet">
    /// <item>
    /// <description>An input should register, no matter how short it is. This is solved by using GLFW's input callbacks
    /// and using "latching" bits. There is a latching bit for presses and releases. They are only reset during the update,
    /// ensuring that a key tap during a lag frame still registers as a press and release.</description>
    /// </item>
    /// <item>
    /// <description>An input should register, no matter how late or early it is. This is solved by using double buffering.</description>
    /// </item>
    /// <item>
    /// <description>Querying the input state must be consistent / idempotent during a frame. Meaning: it doesn't matter
    /// when the state is queried, during a frame it will always return the same result. This is solved by using double buffering.</description>
    /// </item>
    /// <item>
    /// <description>Multiple input actions (press / release) during a frame must all be registered. This is partially
    /// solved by using "latching" bits as explained above. This allows for one press and release action during a frame.
    /// This is a fine compromise since more than one tap per frame is unlikely.</description>
    /// </item>
    /// </list>
    /// Note: isKeyPressed() == true does not imply that isKeyReleased() == false, both can be true.
    /// </summary>
    class Input {
        inline static Input *instance = nullptr;

    public:
        typedef int CallbackRegistrationID;

        typedef std::function<void(float x, float y)> MousePosCallback;
        typedef std::function<void(int button, int action, int mods)> MouseButtonCallback;
        typedef std::function<void(float x, float y)> ScrollCallback;
        typedef std::function<void(int key, int scancode, int action, int mods)> KeyCallback;
        typedef std::function<void(unsigned int codepoint)> CharCallback;

        enum class MouseMode { Release, Capture };

        explicit Input(const glfw::Window &window);

        ~Input();

        /// <returns>The mouse position measured from top-left corner of the viewport.</returns>
        [[nodiscard]] glm::vec2 mousePos() const { return mMousePosRead; }
        /// <returns>The mouse position difference since the last frame.</returns>
        [[nodiscard]] glm::vec2 mouseDelta() const { return mMouseDelta; }
        /// <returns>The scroll wheel position difference since the last frame.</returns>
        [[nodiscard]] glm::vec2 scrollDelta() const { return mScrollDeltaRead; }
        /// <returns>The time difference since the last frame, in seconds.</returns>
        [[nodiscard]] float timeDelta() const { return mTimeDelta; }
        /// <returns>The time since GLFW was initialized, in seconds.</returns>
        [[nodiscard]] double time() const { return mTimeRead; }

        /// <returns>`true` if the mouse is captured (aka. grabbed).</returns>
        [[nodiscard]] bool isMouseCaptured() const { return mMouseCaptured; }

        /// <summary>
        /// Captures the mouse, hiding it and keeping it centered.
        /// </summary>
        void captureMouse();

        /// <returns>`true` if the mouse is **not** captured (aka. grabbed).</returns>
        [[nodiscard]] bool isMouseReleased() const { return !mMouseCaptured; }

        /// <summary>
        /// Releases the mouse, making it visible and usable again.
        /// </summary>
        void releaseMouse();

        /// <summary>
        /// Sets the mouse mode.
        /// </summary>
        /// <param name="mode">The mouse mode to set.</param>
        void setMouseMode(MouseMode mode) {
            if (mode == MouseMode::Release && !isMouseReleased()) {
                releaseMouse();
            }
            if (mode == MouseMode::Capture && !isMouseCaptured() && isWindowFocused()) {
                captureMouse();
            }
            this->mMouseMode = mode;
        }

        /// <returns>The current mouse mode.</returns>
        [[nodiscard]] MouseMode mouseMode() const { return mMouseMode; }

        /// <summary>
        /// Centers the mouse cursor in the window.
        /// </summary>
        void centerMouse() const;

        /// <returns>`true` if the window is focused / selected by the user.</returns>
        [[nodiscard]] bool isWindowFocused() const;

        /// <param name="button">One of `GLFW_MOUSE_BUTTON_*`.</param>
        /// <returns>`true` if the given button is being held down.</returns>
        [[nodiscard]] bool isMouseDown(int button) const {
            return (mMouseButtonsRead[button] & State::PersistentPressedMask) != State::Zero;
        }

        /// <param name="button">One of `GLFW_MOUSE_BUTTON_*`.</param>
        /// <returns>`true` if the given button has been pressed down since the last frame.</returns>
        [[nodiscard]] bool isMousePress(int button) const {
            return (mMouseButtonsRead[button] & State::PressedBit) != State::Zero;
        }

        /// <param name="button">One of `GLFW_MOUSE_BUTTON_*`.</param>
        /// <returns>`true` if the given button has been released up since the last frame.</returns>
        [[nodiscard]] bool isMouseRelease(int button) const {
            return (mMouseButtonsRead[button] & State::ReleasedBit) != State::Zero;
        }

        /// <param name="key">One of `GLFW_KEY_*`.</param>
        /// <returns>`true` if the given key is being held down.</returns>
        [[nodiscard]] bool isKeyDown(int key) const {
            return (mKeysRead[key] & State::PersistentPressedMask) != State::Zero;
        }

        /// <param name="key">The printed key symbol.</param>
        /// <returns>`true` if the given key is being held down.</returns>
        [[nodiscard]] bool isKeyDown(const std::string &key) const {
            if (!mKeyMap.contains(key))
                return false;
            return isKeyDown(mKeyMap.at(key));
        }

        /// <param name="key">One of `GLFW_KEY_*`. Note: This uses the physical position in the US layout.</param>
        /// <returns>`true` if the given key has been pressed down since the last frame.</returns>
        [[nodiscard]] bool isKeyPress(int key) const { return (mKeysRead[key] & State::PressedBit) != State::Zero; }

        /// <param name="key">One of `GLFW_KEY_*`.</param>
        /// <returns>`true` if the given key has been released up since the last frame.</returns>
        [[nodiscard]] bool isKeyRelease(int key) const { return (mKeysRead[key] & State::ReleasedBit) != State::Zero; }

        /// <summary>
        /// Register a mouse position callback.
        /// </summary>
        /// <param name="callback">The callback function to register.</param>
        /// <returns>The ID of the registered callback.</returns>
        CallbackRegistrationID addMousePosCallback(const MousePosCallback &callback) {
            int id = mNextCallbackRegistrationId++;
            mMousePosCallbacks.emplace_back(id, callback);
            return id;
        }

        /// <summary>
        /// Register a mouse button callback.
        /// </summary>
        /// <param name="callback">The callback function to register.</param>
        /// <returns>The ID of the registered callback.</returns>
        CallbackRegistrationID addMouseButtonCallback(const MouseButtonCallback &callback) {
            int id = mNextCallbackRegistrationId++;
            mMouseButtonCallbacks.emplace_back(id, callback);
            return id;
        }

        /// <summary>
        /// Register a mouse wheel callback.
        /// </summary>
        /// <param name="callback">The callback function to register.</param>
        /// <returns>The ID of the registered callback.</returns>
        CallbackRegistrationID addScrollCallback(const ScrollCallback &callback) {
            int id = mNextCallbackRegistrationId++;
            mScrollCallbacks.emplace_back(id, callback);
            return id;
        }

        /// <summary>
        /// Register a keyboard key callback.
        /// </summary>
        /// <param name="callback">The callback function to register.</param>
        /// <returns>The ID of the registered callback.</returns>
        CallbackRegistrationID addKeyCallback(const KeyCallback &callback) {
            int id = mNextCallbackRegistrationId++;
            mKeyCallbacks.emplace_back(id, callback);
            return id;
        }

        /// <summary>
        /// Register a keyboard character callback.
        /// </summary>
        /// <param name="callback">The callback function to register.</param>
        /// <returns>The ID of the registered callback.</returns>
        CallbackRegistrationID addCharCallback(const CharCallback &callback) {
            int id = mNextCallbackRegistrationId++;
            mCharCallbacks.emplace_back(id, callback);
            return id;
        }

        /// <summary>
        /// Remove a callback given its registration id.
        /// </summary>
        /// <param name="registration">The ID of the callback to remove.</param>
        void removeCallback(CallbackRegistrationID &registration);

        /// <summary>
        /// Updates the input state. This should be called once per frame.
        /// </summary>
        void update();

        /// <summary>
        /// Sets a flag that will poll the true input state on the next update.
        /// </summary>
        void invalidate() { mStateInvalid = true; }

        /// <summary>
        /// GLFW key callback.
        /// </summary>
        void onKey(GLFWwindow *window, int key, int scancode, int action, int mods);

        /// <summary>
        /// GLFW cursor position callback.
        /// </summary>
        void onCursorPos(GLFWwindow *window, double x, double y);

        /// <summary>
        /// GLFW mouse button callback.
        /// </summary>
        void onMouseButton(GLFWwindow *window, int button, int action, int mods);

        /// <summary>
        /// GLFW scroll callback.
        /// </summary>
        void onScroll(GLFWwindow *window, double dx, double dy);

        /// <summary>
        /// GLFW char callback.
        /// </summary>
        void onChar(GLFWwindow *window, unsigned int codepoint);

    private:
        template<typename T>
        struct CallbackRegistration {
            CallbackRegistrationID id;
            T callback;
        };

        enum class State : uint8_t {
            Zero = 0,
            ReleasedBit = 0b001,
            PressedBit = 0b010,
            PersistentPressedBit = 0b100,
            ClearMask = static_cast<uint8_t>(~0b011u), // ~(Released | Pressed)
            PersistentPressedMask = PressedBit | PersistentPressedBit,
        };

        friend constexpr State operator|(State lhs, State rhs) {
            return static_cast<State>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
        }

        friend constexpr State operator&(State lhs, State rhs) {
            return static_cast<State>(static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
        }

        friend constexpr State &operator|=(State &lhs, State rhs) { return lhs = lhs | rhs; }

        friend constexpr State &operator&=(State &lhs, State rhs) { return lhs = lhs & rhs; }

        const Window &mWindow;
        double mTimeRead = 0;
        float mTimeDelta = 0;
        bool mMouseCaptured = false;
        MouseMode mMouseMode = MouseMode::Release;
        glm::vec2 mMousePosRead = {};
        glm::vec2 mMousePosWrite = {};
        glm::vec2 mMouseDelta = {};
        glm::vec2 mScrollDeltaRead = {};
        glm::vec2 mScrollDeltaWrite = {};
        std::array<State, 8> mMouseButtonsRead = {};
        std::array<State, 8> mMouseButtonsWrite = {};
        std::array<State, 349> mKeysRead = {};
        std::array<State, 349> mKeysWrite = {};
        std::unordered_map<std::string, int> mKeyMap = {};

        bool mStateInvalid = true;

        int mNextCallbackRegistrationId = 1;
        std::vector<CallbackRegistration<MousePosCallback>> mMousePosCallbacks;
        std::vector<CallbackRegistration<MouseButtonCallback>> mMouseButtonCallbacks;
        std::vector<CallbackRegistration<ScrollCallback>> mScrollCallbacks;
        std::vector<CallbackRegistration<KeyCallback>> mKeyCallbacks;
        std::vector<CallbackRegistration<CharCallback>> mCharCallbacks;

        void *(*mStoredKeyCallback)() = nullptr;
        void *(*mStoredCursorPosCallback)() = nullptr;
        void *(*mStoredMouseButtonCallback)() = nullptr;
        void *(*mStoredScrollCallback)() = nullptr;
        void *(*mStoredCharCallback)() = nullptr;
        void *(*mStoredWindowFocusCallback)() = nullptr;

        void pollCurrentState();
    };
} // namespace glfw
