#include "InstanceAnimationSampler.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../util/Logger.h"
#include "../util/math.h"

InstanceAnimationSampler::InstanceAnimationSampler(const scene::CpuData &cpu_data)
    : mCpuData{cpu_data},
      mAnimationCount{cpu_data.instance_animations.size()},
      mFirstAnimInstanceIdx{cpu_data.instances.size() - cpu_data.instance_animations.size()} {
    // non mesh are appended at the end
    mPrevAnimationIndices.resize(mCpuData.instance_animations.size() + mCpuData.non_mesh_instance_animations.size());
}

glm::mat4 InstanceAnimationSampler::sampleNamedTransform(const std::string& name, float timestamp) {
    if (!mCpuData.non_mesh_instance_animation_map.contains(name))
        return glm::mat4(1.0f);

    auto [instance_index, anim_index] = mCpuData.non_mesh_instance_animation_map.at(name);

    const scene::InstanceAnimation &animation = mCpuData.non_mesh_instance_animations[anim_index];
    const scene::Instance &instance = mCpuData.instances[instance_index];
    const glm::mat4 &default_transform = instance.transform;

    glm::vec3 default_translation;
    glm::quat default_rotation;
    glm::vec3 default_scale;
    util::decomposeTransform(default_transform, &default_translation, &default_rotation, &default_scale);

    size_t prev_index = anim_index + mCpuData.instance_animations.size();
    std::size_t &translation_value_index = mPrevAnimationIndices[prev_index].translation_idx;
    std::size_t &rotation_value_index = mPrevAnimationIndices[prev_index].rotation_idx;
    std::size_t &scale_value_index = mPrevAnimationIndices[prev_index].scale_idx;

    const glm::vec3 translation = sampleTrack<glm::vec3>(
            animation.translation_timestamps, animation.translations, timestamp, default_translation,
            [](const glm::vec3 &a, const glm::vec3 &b, float alpha) { return glm::mix(a, b, alpha); }, translation_value_index
    );
    const glm::quat rotation = sampleTrack<glm::quat>(
            animation.rotation_timestamps, animation.rotations, timestamp, default_rotation,
            [](const glm::quat &a, const glm::quat &b, float alpha) { return glm::slerp(a, b, alpha); }, rotation_value_index
    );
    const glm::vec3 scale = sampleTrack<glm::vec3>(
        animation.scale_timestamps, animation.scales, timestamp, default_scale,
        [](const glm::vec3 &a, const glm::vec3 &b, float alpha) { return glm::mix(a, b, alpha); }, scale_value_index
    );

    return glm::scale(glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation), scale);
}


glm::vec3 InstanceAnimationSampler::sampleNamedTranslation(const std::string& name, float timestamp) {
    glm::vec3 default_translation = glm::vec3{};

    if (!mCpuData.non_mesh_instance_animation_map.contains(name))
        return default_translation;

    auto [instance_index, anim_index] = mCpuData.non_mesh_instance_animation_map.at(name);
    const scene::InstanceAnimation &animation = mCpuData.non_mesh_instance_animations[anim_index];

    size_t prev_index = anim_index + mCpuData.instance_animations.size();
    std::size_t &value_index = mPrevAnimationIndices[prev_index].translation_idx;

    const glm::vec3 translation = sampleTrack<glm::vec3>(
            animation.translation_timestamps, animation.translations, timestamp, default_translation,
            [](const glm::vec3 &a, const glm::vec3 &b, float alpha) { return glm::mix(a, b, alpha); }, value_index
    );

    return translation;
}

glm::quat InstanceAnimationSampler::sampleNamedRotation(const std::string& name, float timestamp) {
    glm::quat default_rotation = glm::quat(1.0, 0.0, 0.0, 0.0);

    if (!mCpuData.non_mesh_instance_animation_map.contains(name))
        return default_rotation;

    auto [instance_index, anim_index] = mCpuData.non_mesh_instance_animation_map.at(name);
    const scene::InstanceAnimation &animation = mCpuData.non_mesh_instance_animations[anim_index];

    size_t prev_index = anim_index + mCpuData.instance_animations.size();
    std::size_t &value_index = mPrevAnimationIndices[prev_index].rotation_idx;

    const glm::quat rotation = sampleTrack<glm::quat>(
            animation.rotation_timestamps, animation.rotations, timestamp, default_rotation,
            [](const glm::quat &a, const glm::quat &b, float alpha) { return glm::slerp(a, b, alpha); }, value_index
    );

    return rotation;
}

glm::vec3 InstanceAnimationSampler::sampleNamedScale(const std::string& name, float timestamp) {
    glm::vec3 default_scale = glm::vec3{1.0, 1.0, 1.0};

    if (!mCpuData.non_mesh_instance_animation_map.contains(name))
        return default_scale;

    auto [instance_index, anim_index] = mCpuData.non_mesh_instance_animation_map.at(name);
    const scene::InstanceAnimation &animation = mCpuData.non_mesh_instance_animations[anim_index];

    size_t prev_index = anim_index + mCpuData.instance_animations.size();
    std::size_t &value_index = mPrevAnimationIndices[prev_index].scale_idx;

    const glm::vec3 scale = sampleTrack<glm::vec3>(
            animation.scale_timestamps, animation.scales, timestamp, default_scale,
            [](const glm::vec3 &a, const glm::vec3 &b, float alpha) { return glm::mix(a, b, alpha); }, value_index
    );

    return scale;
}

std::vector<glm::mat4> InstanceAnimationSampler::sampleAnimatedInstanceTransforms(float timestamp) {
    std::vector<glm::mat4> transforms;
    transforms.reserve(mAnimationCount);

    for (std::size_t i{0}; i < mAnimationCount; ++i) {
        const glm::mat4 transform = sampleInstanceAnimation(i, timestamp);
        transforms.push_back(transform);
    }

    return transforms;
}

glm::mat4 InstanceAnimationSampler::sampleInstanceAnimation(std::size_t anim_idx, float timestamp) {
    const std::size_t instance_idx = mFirstAnimInstanceIdx + anim_idx;
    const glm::mat4 &default_transform = mCpuData.instances[instance_idx].transform;

    const glm::vec3 translation = sampleInstanceTranslation(anim_idx, timestamp, default_transform);
    const glm::quat rotation = sampleInstanceRotation(anim_idx, timestamp, default_transform);

    return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation);
}

glm::vec3 InstanceAnimationSampler::sampleInstanceTranslation(
        std::size_t anim_idx, float timestamp, const glm::mat4 &default_transform
) {
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

glm::quat InstanceAnimationSampler::sampleInstanceRotation(
        std::size_t anim_idx, float timestamp, const glm::mat4 &default_transform
) {
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
T InstanceAnimationSampler::sampleTrack(
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
