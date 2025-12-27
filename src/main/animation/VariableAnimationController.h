#pragma once

#include <algorithm>
#include <format>
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "../util/Logger.h"

// Add lerp functions for other types if needed
inline float lerpValue(float a, float b, float alpha) { return a + (b - a) * alpha; }
inline glm::vec3 lerpValue(const glm::vec3 &a, const glm::vec3 &b, float alpha) { return glm::mix(a, b, alpha); }

template<typename T>
struct Keyframe {
    float timestamp;
    T value;
};

struct ITrack {
    virtual ~ITrack() = default;

    /// <summary>
    /// Evaluates the track at the specified timestamp and writes the result into the bound target
    /// variable.
    /// </summary>
    /// <param name="timestamp">Timestamp to evaluate for (same units as keyframes).</param>
    virtual void apply(float timestamp) = 0;

    /// <summary>
    /// Returns the runtime value type of the variable bound to the track (e.g., typeid(float)).
    /// </summary>
    /// <returns>std::type_index specifying the bound value type.</returns>
    virtual std::type_index valueType() const = 0;
};

template<typename T>
class Track final : public ITrack {
private:
    T *mTargetPtr;
    std::vector<Keyframe<T>> mKeyframes;

public:
    /// <summary>
    /// Constructs a track bound to a target variable that is supposed to be animated.
    /// </summary>
    /// <param name="target">Target variable to animate. Must outlive the Track.</param>
    explicit Track(T &target) : mTargetPtr(&target) {
        Logger::check(mTargetPtr != nullptr, "Attempted to create animation track for nullptr");
    }

    Track(const Track &) = delete;
    Track &operator=(const Track &) = delete;
    Track(Track &&) noexcept = default;
    Track &operator=(Track &&) noexcept = default;

    std::type_index valueType() const override { return typeid(T); }

    /// <summary>
    /// Adds a keyframe while keeping the keyframe list sorted by the timestamps. Overwrites the
    /// value if a keyframe with the given timestamp already exists.
    /// </summary>
    /// <param name="timestamp">Keyframe timestamp.</param>
    /// <param name="value">Value at the given timestamp.</param>
    void addKeyframe(float timestamp, const T &value) {
        auto it = std::lower_bound(
                mKeyframes.begin(), mKeyframes.end(), timestamp,
                [](const Keyframe<T> &keyframe, float time) { return keyframe.timestamp < time; }
        );

        if (it != mKeyframes.end() && it->timestamp == timestamp) {
            it->value = value;
        } else {
            mKeyframes.insert(it, Keyframe<T>{timestamp, value});
        }
    }

    /// <summary>
    /// Updates the bound target variable for the given timestamp by interpolation between keyframes.
    /// </summary>
    /// <param name="timestamp">Timestamp to evaluate values for (same units as keyframes).</param>
    void apply(float timestamp) override {
        if (mKeyframes.empty())
            return;

        // Clamp outside range
        if (timestamp <= mKeyframes.front().timestamp) {
            *mTargetPtr = mKeyframes.front().value;
            return;
        }
        if (timestamp >= mKeyframes.back().timestamp) {
            *mTargetPtr = mKeyframes.back().value;
            return;
        }

        auto it = std::upper_bound(
                mKeyframes.begin(), mKeyframes.end(), timestamp,
                [](float time, const Keyframe<T> &keyframe) { return time < keyframe.timestamp; }
        );

        const Keyframe<T> &keyframe_b = *it;
        const Keyframe<T> &keyframe_a = *(it - 1);

        const float denom = (keyframe_b.timestamp - keyframe_a.timestamp);
        const float alpha = (denom > 0.0f) ? (timestamp - keyframe_a.timestamp) / denom : 0.0f;

        *mTargetPtr = lerpValue(keyframe_a.value, keyframe_b.value, alpha);
    }
};

class VariableAnimationController {
private:
    std::unordered_map<void *, std::unique_ptr<ITrack>> mTracks;

public:
    /// <summary>
    /// Creates a new track for the specified target variable to be animated. Throws if a track for
    /// the same target variable already exists.
    /// </summary>
    /// <typeparam name="T">Value type of the target variable.</typeparam>
    /// <param name="target_var">Variable to animate. Must outlive the controller.</param>
    template<typename T>
    void createTrack(T &target_var) {
        void *key = static_cast<void *>(&target_var);

        if (mTracks.find(key) != mTracks.end())
            Logger::fatal(std::format("There already exists an animation track for variable at {}", key));

        mTracks.emplace(key, std::make_unique<Track<T>>(target_var));
    }

    /// <summary>
    /// Returns the existing track registered for the given target variable. Fails if no track
    /// exists for the target variable, or if the stored track type does not match.
    /// </summary>
    /// <typeparam name="T">Expected value type of the track.</typeparam>
    /// <param name="target_var">Variable of the track to be returned.</param>
    /// <returns>Reference to the stored Track bound to the target variable.</returns>
    template<typename T>
    Track<T> &track(T &target_var) {
        void *key = static_cast<void *>(&target_var);

        auto it = mTracks.find(key);

        if (it == mTracks.end())
            Logger::fatal(std::format("There exists no animation track for variable at {}", key));

        if (it->second->valueType() != typeid(T))
            Logger::fatal("Track type mismatch for same address");

        return *static_cast<Track<T> *>(it->second.get());
    }

    /// <summary>
    /// Evaluates all tracks at the given timestamp and updates the values of their bound target
    /// variables.
    /// </summary>
    /// <param name="timestamp">Timestamp to evaluate values for (same units as keyframes).</param>
    void update(float timestamp) {
        for (auto &[_, track]: mTracks)
            track->apply(timestamp);
    }
};
