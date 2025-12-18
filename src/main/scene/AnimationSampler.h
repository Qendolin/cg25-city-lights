#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "Scene.h"

namespace scene {

    /// <summary>
    /// Caches the last used keyframe index for an instance's animation.
    /// This avoids searching the keyframe array from the beginning at every frame.
    /// </summary>
    struct InstanceAnimationCursor {
        std::size_t translation_idx{};
        std::size_t rotation_idx{};
    };


    /// <summary>
    /// A utility class for sampling instance animations at a specific timestamp.
    /// </summary>
    class AnimationSampler {
    public:
        bool loop = true;

        [[nodiscard]] std::vector<glm::mat4> sampleAnimatedInstanceTransforms(
                const CpuData &cpu_data, float timestamp, std::vector<InstanceAnimationCursor> &animation_cursor_cache
        ) const;

    private:
        glm::vec3 sampleTranslation(
                const InstanceAnimation &anim,
                std::size_t anim_idx,
                float timestamp,
                const glm::mat4 &prev_transform,
                std::vector<InstanceAnimationCursor> &anim_cursor_cache
        ) const;

        glm::quat sampleRotation(
                const InstanceAnimation &anim,
                std::size_t anim_idx,
                float timestamp,
                const glm::mat4 &prev_transform,
                std::vector<InstanceAnimationCursor> &anim_cursor_cache
        ) const;
    };

} // namespace scene
