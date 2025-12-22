#include "AnimationSampler.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

#include "../util/Logger.h"

namespace scene {

    AnimationSampler::AnimationSampler(const CpuData &cpu_data)
        : mCpuData{cpu_data},
          mAnimationCount{cpu_data.instance_animations.size()},
          mFirstAnimInstanceIdx{cpu_data.instances.size() - cpu_data.instance_animations.size()} {
        mPrevAnimationIndices.resize(mCpuData.instance_animations.size());
    }

    glm::mat4 AnimationSampler::sampleAnimatedCameraTransform(float timestamp) {
        if (!mCpuData.animated_camera_exists)
            Logger::fatal("Attempted to sample non-existent camera animation");

        const InstanceAnimation &cam_animation = mCpuData.camera_animation;
        const Instance &cam_instance = mCpuData.instances[mCpuData.animated_camera_index];
        const glm::mat4 &default_transform = cam_instance.transform;
        const glm::vec3 default_translation = glm::vec3(default_transform[3]);
        const glm::quat default_rotation = glm::quat_cast(default_transform);
        std::size_t &translation_value_index = mPrevCamAnimIndex.translation_idx;
        std::size_t &rotation_value_index = mPrevCamAnimIndex.rotation_idx;

        const glm::vec3 translation = sampleTrack<glm::vec3>(
                cam_animation.translation_timestamps, cam_animation.translations, timestamp, default_translation,
                [](const glm::vec3 &a, const glm::vec3 &b, float alpha) { return glm::mix(a, b, alpha); },
                translation_value_index
        );
        const glm::quat rotation = sampleTrack<glm::quat>(
                cam_animation.rotation_timestamps, cam_animation.rotations, timestamp, default_rotation,
                [](const glm::quat &a, const glm::quat &b, float alpha) { return glm::slerp(a, b, alpha); },
                rotation_value_index
        );

        return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation);
    }

    std::vector<glm::mat4> AnimationSampler::sampleAnimatedInstanceTransforms(float timestamp) {
        std::vector<glm::mat4> transforms;
        transforms.reserve(mAnimationCount);

        for (std::size_t i{0}; i < mAnimationCount; ++i) {
            const glm::mat4 transform = sampleInstanceAnimation(i, timestamp);
            transforms.push_back(transform);
        }

        return transforms;
    }

    glm::mat4 AnimationSampler::sampleInstanceAnimation(std::size_t anim_idx, float timestamp) {
        const std::size_t instance_idx = mFirstAnimInstanceIdx + anim_idx;
        const glm::mat4 &default_transform = mCpuData.instances[instance_idx].transform;

        const glm::vec3 translation = sampleInstanceTranslation(anim_idx, timestamp, default_transform);
        const glm::quat rotation = sampleInstanceRotation(anim_idx, timestamp, default_transform);

        return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation);
    }

    glm::vec3 AnimationSampler::sampleInstanceTranslation(std::size_t anim_idx, float timestamp, const glm::mat4 &default_transform) {
        const std::vector<float> &timestamps = mCpuData.instance_animations[anim_idx].translation_timestamps;
        const std::vector<glm::vec3> &translations = mCpuData.instance_animations[anim_idx].translations;
        const glm::vec3 default_translation = glm::vec3(default_transform[3]);

        // Updated by sampleTrack!
        std::size_t &value_index = mPrevAnimationIndices[anim_idx].translation_idx;

        return sampleTrack<glm::vec3>(
                timestamps, translations, timestamp, default_translation,
                [](const glm::vec3 &a, const glm::vec3 &b, float alpha) { return glm::mix(a, b, alpha); }, value_index
        );
    }

    glm::quat AnimationSampler::sampleInstanceRotation(std::size_t anim_idx, float timestamp, const glm::mat4 &default_transform) {
        const std::vector<float> &timestamps = mCpuData.instance_animations[anim_idx].rotation_timestamps;
        const std::vector<glm::quat> &rotations = mCpuData.instance_animations[anim_idx].rotations;
        const glm::quat default_rotation = glm::quat_cast(default_transform);

        // Updated by sampleTrack!
        std::size_t &value_index = mPrevAnimationIndices[anim_idx].rotation_idx;

        return sampleTrack<glm::quat>(
                timestamps, rotations, timestamp, default_rotation,
                [](const glm::quat &a, const glm::quat &b, float alpha) { return glm::slerp(a, b, alpha); }, value_index
        );
    }

    template<class T, class LerpFn>
    T AnimationSampler::sampleTrack(
            const std::vector<float> &timestamps,
            const std::vector<T> &values,
            float timestamp,
            T default_value,
            LerpFn &&lerp_function,
            // Upon call, value_index should be the index that was the start of the interpolation
            // interval in the last call. During the call it's updated to remain that index.
            std::size_t &value_index
    ) {
        // Return the default value if there is no animation or it hasn't started yet
        if (values.empty() || timestamp < timestamps.front()) {
            value_index = 0;
            return default_value;
        }

        // Return the last value if the animation has ended
        if (timestamp >= timestamps.back())
            return values.back();

        // Search the next index either forward or backward, depending on the playback direction
        if (timestamp >= timestamps[value_index + 1])
            while (value_index < (timestamps.size() - 1) && timestamp >= timestamps[value_index + 1])
                ++value_index;
        else if (timestamp < timestamps[value_index])
            while (timestamp < timestamps[value_index] && value_index > 0)
                --value_index;

        const float anim_ts_0 = timestamps[value_index];
        const float anim_ts_1 = timestamps[value_index + 1];
        const float alpha = (timestamp - anim_ts_0) / (anim_ts_1 - anim_ts_0);

        return lerp_function(values[value_index], values[value_index + 1], alpha);
    }
} // namespace scene
