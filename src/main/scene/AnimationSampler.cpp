#include "AnimationSampler.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../util/Logger.h"

namespace scene {

    std::vector<glm::mat4> AnimationSampler::sampleAnimatedInstanceTransforms(
            const CpuData &cpu_data, float timestamp, std::vector<InstanceAnimationCursor> &anim_cursor_cache
    ) const {
        std::vector<glm::mat4> transforms;
        transforms.reserve(cpu_data.instance_animations.size());

        const std::size_t anim_cursor_count = anim_cursor_cache.size();
        const std::size_t anim_count = cpu_data.instance_animations.size();
        const std::size_t first_anim_instance_idx = cpu_data.instances.size() - cpu_data.instance_animations.size();

        if (anim_cursor_count != anim_count) {
            Logger::warning(std::format(
                    "Ignoring animations because the animation cursor cache is of size {} but there are {} animations",
                    anim_cursor_count, anim_count
            ));

            for (std::size_t anim_idx{0}; anim_idx < anim_count; ++anim_idx) {
                const std::size_t instance_idx = first_anim_instance_idx + anim_idx;
                const Instance &instance = cpu_data.instances[instance_idx];
                transforms.push_back(instance.transform);
            }

            return transforms;
        }

        for (std::size_t anim_idx{0}; anim_idx < anim_count; ++anim_idx) {
            const InstanceAnimation &anim = cpu_data.instance_animations[anim_idx];
            const std::size_t instance_idx = first_anim_instance_idx + anim_idx;
            const Instance &instance = cpu_data.instances[instance_idx];
            const glm::mat4 &prev_transform = instance.transform;

            const glm::vec3 translation = sampleTranslation(anim, anim_idx, timestamp, prev_transform, anim_cursor_cache);
            const glm::quat rotation = sampleRotation(anim, anim_idx, timestamp, prev_transform, anim_cursor_cache);
            glm::mat4 transform(1.0f);
            transform = glm::translate(transform, translation) * glm::mat4_cast(rotation);
            transforms.push_back(transform);
        }

        return transforms;
    }

    glm::vec3 AnimationSampler::sampleTranslation(
            const InstanceAnimation &anim,
            std::size_t anim_idx,
            float timestamp,
            const glm::mat4 &prev_transform,
            std::vector<InstanceAnimationCursor> &anim_cursor_cache
    ) const {
        // Return the instance's unmodified translation if there is no translation animation or it
        // hasn't started yet
        if (anim.translations.empty() || timestamp < anim.translation_timestamps.front())
            return glm::vec3(prev_transform[3]);

        bool end_reached = timestamp >= anim.translation_timestamps.back();
        if (loop) {
            timestamp = std::fmodf(timestamp, anim.translation_timestamps.back());
            if (end_reached) anim_cursor_cache[anim_idx].translation_idx = 0;
        } else if (end_reached) {
            // Return the last translation value if the animation has ended
            return anim.translations.back();
        }

        // Linearly interpolate if the animation is active
        std::size_t index = anim_cursor_cache[anim_idx].translation_idx;

        while (timestamp >= anim.translation_timestamps[index + 1] && index < (anim.translations.size() - 1))
            ++index;

        anim_cursor_cache[anim_idx].translation_idx = index;

        float anim_ts_0 = anim.translation_timestamps[index];
        float anim_ts_1 = anim.translation_timestamps[index + 1];
        const float alpha = (timestamp - anim_ts_0) / (anim_ts_1 - anim_ts_0);

        return glm::mix(anim.translations[index], anim.translations[index + 1], alpha);
    }

    glm::quat AnimationSampler::sampleRotation(
            const InstanceAnimation &anim,
            std::size_t anim_idx,
            float timestamp,
            const glm::mat4 &prev_transform,
            std::vector<InstanceAnimationCursor> &anim_cursor_cache
    ) const {
        // Return the instance's unmodified rotation if there is no rotation animation or it
        // hasn't started yet
        if (anim.rotations.empty() || timestamp < anim.rotation_timestamps.front())
            return glm::quat_cast(prev_transform);

        bool end_reached = timestamp >= anim.rotation_timestamps.back();
        if (loop) {
            timestamp = std::fmodf(timestamp, anim.rotation_timestamps.back());
            if (end_reached) anim_cursor_cache[anim_idx].rotation_idx = 0;
        } else if (end_reached) {
            // Return the last translation value if the animation has ended
            return anim.translations.back();
        }

        // Linearly interpolate if the animation is active
        std::size_t index = anim_cursor_cache[anim_idx].rotation_idx;

        while (timestamp >= anim.rotation_timestamps[index + 1] && index < (anim.rotations.size() - 1))
            ++index;

        float anim_ts_0 = anim.rotation_timestamps[index];
        float anim_ts_1 = anim.rotation_timestamps[index + 1];
        const float alpha = (timestamp - anim_ts_0) / (anim_ts_1 - anim_ts_0);

        return glm::normalize(glm::lerp(anim.rotations[index], anim.rotations[index + 1], alpha));
    }
} // namespace scene
