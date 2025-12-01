#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "Scene.h"

namespace scene {

    struct InstanceAnimationCursor {
        std::size_t translation_idx{};
        std::size_t rotation_idx{};
    };

    // TODO: Summary
    class AnimationSampler {
    public:
        static std::vector<glm::mat4> sampleAnimatedInstanceTransforms(
                const CpuData &cpu_data, float timestamp, std::vector<InstanceAnimationCursor> &animation_cursor_cache
        );

    private:
        static glm::vec3 sampleTranslation(
                const InstanceAnimation &anim,
                const std::size_t anim_idx,
                const float timestamp,
                const glm::mat4 &prev_transform,
                std::vector<InstanceAnimationCursor> &anim_cursor_cache
        );

        static glm::quat sampleRotation(
                const InstanceAnimation &anim,
                const std::size_t anim_idx,
                const float timestamp,
                const glm::mat4 &prev_transform,
                std::vector<InstanceAnimationCursor> &anim_cursor_cache
        );
    };

} // namespace scene
