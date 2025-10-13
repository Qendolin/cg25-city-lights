#include "Input.h"

#include <GLFW/glfw3.h>
#include <algorithm>

#include "../util/Logger.h"
#include "Window.h"

namespace glfw {
    Input::Input(const glfw::Window &window) : mWindow(window) {
        if (instance != nullptr) {
            Logger::fatal("Only one instance of Input can be created");
        }
        instance = this;

        for (int key = GLFW_KEY_SPACE; key < mKeysWrite.size(); key++) {
            int sc = glfwGetKeyScancode(key);
            if (sc == -1)
                continue;
            const char *name = glfwGetKeyName(key, sc);
            if (name != nullptr) {
                mKeyMap[name] = key;
            }
        }

        auto win_ptr = mWindow;
        mStoredKeyCallback = reinterpret_cast<void *(*) ()>( //
                glfwSetKeyCallback(
                        win_ptr,
                        [](GLFWwindow *window, int key, int scancode, int action, int mods) { //
                            instance->onKey(window, key, scancode, action, mods);
                        }
                )
        );
        mStoredCursorPosCallback = reinterpret_cast<void *(*) ()>( //
                glfwSetCursorPosCallback(
                        win_ptr, [](GLFWwindow *window, double x, double y) { instance->onCursorPos(window, x, y); }
                )
        );
        mStoredMouseButtonCallback = reinterpret_cast<void *(*) ()>( //
                glfwSetMouseButtonCallback(
                        win_ptr,
                        [](GLFWwindow *window, int button, int action, int mods) { //
                            instance->onMouseButton(window, button, action, mods);
                        }
                )
        );
        mStoredScrollCallback = reinterpret_cast<void *(*) ()>( //
                glfwSetScrollCallback(
                        win_ptr,
                        [](GLFWwindow *window, double dx, double dy) { //
                            instance->onScroll(window, dx, dy);
                        }
                )
        );
        mStoredCharCallback = reinterpret_cast<void *(*) ()>( //
                glfwSetCharCallback(
                        win_ptr,
                        [](GLFWwindow *window, unsigned int codepoint) { //
                            instance->onChar(window, codepoint);
                        }
                )
        );
        mStoredWindowFocusCallback = reinterpret_cast<void *(*) ()>( //
                glfwSetWindowFocusCallback(
                        win_ptr,
                        [](GLFWwindow * /*window*/, int /*focused*/) { //
                            instance->invalidate();
                        }
                )
        );
    }

    Input::~Input() {
        glfwSetKeyCallback(mWindow, reinterpret_cast<GLFWkeyfun>(mStoredKeyCallback));
        glfwSetCursorPosCallback(mWindow, reinterpret_cast<GLFWcursorposfun>(mStoredCursorPosCallback));
        glfwSetMouseButtonCallback(mWindow, reinterpret_cast<GLFWmousebuttonfun>(mStoredMouseButtonCallback));
        glfwSetScrollCallback(mWindow, reinterpret_cast<GLFWscrollfun>(mStoredScrollCallback));
        glfwSetCharCallback(mWindow, reinterpret_cast<GLFWcharfun>(mStoredCharCallback));
        glfwSetWindowFocusCallback(mWindow, reinterpret_cast<GLFWwindowfocusfun>(mStoredWindowFocusCallback));

        instance = nullptr;
    }

    void Input::pollCurrentState() {
        mStateInvalid = false;

        for (int key = GLFW_KEY_SPACE; key < mKeysWrite.size(); key++) {
            int state = glfwGetKey(mWindow, key);
            mKeysWrite[key] = state == GLFW_PRESS ? State::PersistentPressedMask : State::Zero;
        }

        for (int button = GLFW_MOUSE_BUTTON_1; button < mMouseButtonsWrite.size(); button++) {
            int state = glfwGetMouseButton(mWindow, button);
            mMouseButtonsWrite[button] = state == GLFW_PRESS ? State::PersistentPressedMask : State::Zero;
        }

        double mouse_x = 0, mouse_y = 0;
        glfwGetCursorPos(mWindow, &mouse_x, &mouse_y);
        mMousePosWrite = {mouse_x, mouse_y};
        // No mouse delta
        mMousePosRead = mMousePosWrite;

        mMouseCaptured = glfwGetInputMode(mWindow, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;

        // No time delta
        mTimeRead = glfwGetTime();
    }

    void Input::update() {
        glfwPollEvents();

        if (mStateInvalid) {
            pollCurrentState();
        }

        double time = glfwGetTime();
        mTimeDelta = static_cast<float>(time - mTimeRead);
        mTimeRead = time;

        mMouseDelta = mMousePosWrite - mMousePosRead;
        mMousePosRead = mMousePosWrite;

        mScrollDeltaRead = mScrollDeltaWrite;
        mScrollDeltaWrite = glm::vec2(0.0f);

        // During a frame key events are captured and flags set in the keysWrite_ buffer.
        // After a frame the keysWrite_ buffer is copied to the keysRead_ buffer and then cleared.
        std::copy(std::begin(mKeysWrite), std::end(mKeysWrite), std::begin(mKeysRead));
        // The state changes (pressed, released) are cleared but the current state is kept
        for (auto &state: mKeysWrite) {
            state &= State::ClearMask;
        }

        // Same for mouse buttons
        std::copy(std::begin(mMouseButtonsWrite), std::end(mMouseButtonsWrite), std::begin(mMouseButtonsRead));
        for (auto &state: mMouseButtonsWrite) {
            state &= State::ClearMask;
        }
    }

    void Input::captureMouse() {
        glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mMouseCaptured = true;
    }

    void Input::releaseMouse() {
        glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        mMouseCaptured = false;
    }

    void Input::centerMouse() const {
        int w, h;
        glfwGetWindowSize(mWindow, &w, &h);
        glfwSetCursorPos(mWindow, w / 2, h / 2);
    }

    bool Input::isWindowFocused() const { return glfwGetWindowAttrib(mWindow, GLFW_FOCUSED) == GLFW_TRUE; }

    void Input::onKey(GLFWwindow * /*window*/, int key, int scancode, int action, int mods) {
        if (key < 0 || key >= mKeysWrite.size())
            return; // special keys, e.g.: mute sound
        if (action == GLFW_PRESS) {
            // set the pressed and down bit
            mKeysWrite[key] |= State::PressedBit;
            mKeysWrite[key] |= State::PersistentPressedBit;
        }

        if (action == GLFW_RELEASE) {
            // set the released and clear the down bit
            mKeysWrite[key] |= State::ReleasedBit;
            mKeysWrite[key] &= static_cast<State>(~static_cast<uint8_t>(State::PersistentPressedBit));
        }

        for (auto &&reg: mKeyCallbacks)
            reg.callback(key, scancode, action, mods);
    }

    void Input::onCursorPos(GLFWwindow * /*window*/, double x, double y) {
        mMousePosWrite.x = static_cast<float>(x);
        mMousePosWrite.y = static_cast<float>(y);

        for (auto &&reg: mMousePosCallbacks)
            reg.callback(static_cast<float>(x), static_cast<float>(y));
    }

    void Input::onMouseButton(GLFWwindow * /*window*/, int button, int action, int mods) {
        if (action == GLFW_PRESS) {
            // set the pressed and down bit
            mMouseButtonsWrite[button] |= State::PressedBit;
            mMouseButtonsWrite[button] |= State::PersistentPressedBit;
        }

        if (action == GLFW_RELEASE) {
            // set the released and clear the down bit
            mMouseButtonsWrite[button] |= State::ReleasedBit;
            mMouseButtonsWrite[button] &= static_cast<State>(~static_cast<uint8_t>(State::PersistentPressedBit));
        }

        for (auto &&reg: mMouseButtonCallbacks)
            reg.callback(button, action, mods);
    }

    void Input::onScroll(GLFWwindow * /*window*/, double dx, double dy) {
        mScrollDeltaWrite.x += static_cast<float>(dx);
        mScrollDeltaWrite.y += static_cast<float>(dy);

        for (auto &&reg: mScrollCallbacks)
            reg.callback(static_cast<float>(dx), static_cast<float>(dy));
    }

    void Input::onChar(GLFWwindow * /*window*/, unsigned int codepoint) {
        for (auto &&reg: mCharCallbacks)
            reg.callback(codepoint);
    }

    // a little helper function
    template<typename T>
    void removeCallbackFromVec(std::vector<T> &vec, Input::CallbackRegistrationID id) {
        vec.erase(std::remove_if(vec.begin(), vec.end(), [&id](T &elem) { return elem.id == id; }), vec.end());
    }

    void Input::removeCallback(CallbackRegistrationID &registration) {
        if (registration == 0) {
            Logger::warning("removeCallback called with invalid registration id (0)");
        }
        removeCallbackFromVec(mMousePosCallbacks, registration);
        removeCallbackFromVec(mMouseButtonCallbacks, registration);
        removeCallbackFromVec(mScrollCallbacks, registration);
        removeCallbackFromVec(mKeyCallbacks, registration);
        removeCallbackFromVec(mCharCallbacks, registration);
        registration = 0;
    }
} // namespace glfw
