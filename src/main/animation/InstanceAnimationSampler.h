#pragma once

#include <float.h>
#include <glm/glm.hpp>
#include <vector>

#include "../scene/Scene.h"

/// <summary>
/// Used to store the last used keyframe index for an instance's animation.
/// This avoids searching the keyframe array from the beginning at every frame.
/// </summary>
struct InstanceAnimationIndex {
    std::size_t translation_idx{};
    std::size_t rotation_idx{};
};

/// <summary>
/// A class for sampling instance animations at a specific timestamp.
/// Becomes invalid if the CPU data of the referenced scene is modified.
/// </summary>
class InstanceAnimationSampler {
private:
    const scene::CpuData &mCpuData;
    const std::size_t mAnimationCount;
    const std::size_t mFirstAnimInstanceIdx;
    std::vector<InstanceAnimationIndex> mPrevAnimationIndices;
    InstanceAnimationIndex mPrevCamAnimIndex;
    InstanceAnimationIndex mPrevBlobAnimIndex;

public:
    InstanceAnimationSampler(const scene::CpuData &cpu_data);

    /// <summary>
    /// Samples the transform matrix of the animated instance associated with the camera for the
    /// specified timestamp. Throws if the loaded scene data contains no animated camera node.
    /// </summary>
    /// <param name="timestamp">The timestamp to sample for in seconds</param>
    /// <returns>The interpolated transform matrix for the animated camera instance.</returns>
    [[nodiscard]] glm::mat4 sampleAnimatedCameraTransform(float timestamp);

    /// <summary>
    /// Samples the transform matrix of the animated instance associated with the blob for the
    /// specified timestamp. Throws if the loaded scene data contains no animated blob node.
    /// </summary>
    /// <param name="timestamp">The timestamp to sample for in seconds</param>
    /// <returns>The interpolated transform matrix for the animated blob instance.</returns>
    [[nodiscard]] glm::mat4 sampleAnimatedBlobTransform(float timestamp);

    /// <summary>
    /// Samples all transform matrices of all instance animation instances stored in the scene (blob
    /// and camera animations are stored separately) for the specified timestamp.
    /// </summary>
    /// <param name="timestamp">The timestamp to sample for in seconds</param>
    /// <returns>A list of interpolated transform matrices in the same order the instance animations
    /// are stored.</returns>
    [[nodiscard]] std::vector<glm::mat4> sampleAnimatedInstanceTransforms(float timestamp);

private:
    [[nodiscard]] glm::mat4 sampleInstanceAnimation(std::size_t anim_idx, float timestamp);
    [[nodiscard]] glm::vec3 sampleInstanceTranslation(std::size_t anim_idx, float timestamp, const glm::mat4 &default_transform);
    [[nodiscard]] glm::quat sampleInstanceRotation(std::size_t anim_idx, float timestamp, const glm::mat4 &default_transform);

    template<class T, class LerpFn>
    [[nodiscard]] T sampleTrack(
            const std::vector<float> &timestamps,
            const std::vector<T> &values,
            float timestamp,
            T default_value,
            LerpFn &&lerp_function,
            std::size_t &anim_index
    );
};
