#pragma once

#include <float.h>
#include <glm/glm.hpp>
#include <vector>

#include "Scene.h"

namespace scene {

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
    class AnimationSampler {
    private:
        const CpuData &mCpuData;
        const std::size_t mAnimationCount;
        const std::size_t mFirstAnimInstanceIdx;
        std::vector<InstanceAnimationIndex> mPrevAnimationIndices;
        InstanceAnimationIndex mPrevCamAnimIndex;

    public:
        AnimationSampler(const CpuData &cpu_data);

        [[nodiscard]] std::vector<glm::mat4> sampleAnimatedInstanceTransforms(float timestamp);
        [[nodiscard]] glm::mat4 sampleAnimatedCameraTransform(float timestamp);

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

} // namespace scene
