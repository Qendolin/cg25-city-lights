#include "ShadowCaster.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../debug/Annotation.h"

inline glm::vec3 pickSafeUpVector(const glm::vec3& direction, glm::vec3 up = {0.0f, 1.0f, 0.0f}) {
    float dot = glm::dot(direction, up);
    if (dot < -0.99 || dot > 0.99) {
        // direction is too close to up vector, pick another one
        glm::vec3 abs = glm::abs(up);
        if (abs.x < abs.y && abs.x < abs.z)
            up = {1, 0, 0};
        else if (abs.y < abs.z)
            up = {0, 1, 0};
        else
            up = {0, 0, 1};
    }
    return up;
}

ShadowCaster::ShadowCaster(const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution)
    : mResolution(resolution) {
    mDepthImage = Image::create(
            allocator,
            {
                .format = DepthFormat,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
                .type = vk::ImageType::e2D,
                .width = resolution,
                .height = resolution,
                .mipLevels = 1,
            }
    );
    util::setDebugName(device, *mDepthImage, "shadow_depth_image");
    mDepthImageView = mDepthImage.createDefaultView(device);

    util::setDebugName(device, *mDepthImageView, "shadow_depth_image_view");
    mFramebuffer = Framebuffer{vk::Extent2D{resolution, resolution}};
    mFramebuffer.depthAttachment = {
        .image = *mDepthImage,
        .view = *mDepthImageView,
        .format = mDepthImage.info.format,
        .extents = {mDepthImage.info.width, mDepthImage.info.height},
        .range = {.aspectMask = vk::ImageAspectFlagBits::eDepth, .levelCount = 1, .layerCount = 1},
    };
}

SimpleShadowCaster::SimpleShadowCaster(
        const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution, float radius, float start, float end
)
    : ShadowCaster(device, allocator, resolution), mRadius(radius), mStart(start), mEnd(end) {}

void SimpleShadowCaster::setExtentRadius(float radius) {
    mRadius = radius;
    updateProjectionMatrix();
}

void SimpleShadowCaster::setExtentDepth(float start, float end) {
    mStart = start;
    mEnd = end;
    updateProjectionMatrix();
}

void SimpleShadowCaster::setExtentDepthStart(float start) {
    mStart = start;
    updateProjectionMatrix();
}

void SimpleShadowCaster::setExtentDepthEnd(float end) {
    mEnd = end;
    updateProjectionMatrix();
}

void SimpleShadowCaster::setExtents(float radius, float start, float end) {
    mRadius = radius;
    mStart = start;
    mEnd = end;
    updateProjectionMatrix();
}

void SimpleShadowCaster::updateProjectionMatrix() {
    float half_extent = 0.5f * mRadius;
    // swap near and far plane for reverse z
    projectionMatrix = glm::ortho(-half_extent, half_extent, -half_extent, half_extent, mEnd, mStart);
}

void SimpleShadowCaster::lookAt(const glm::vec3 &target, const glm::vec3 &direction, float distance, const glm::vec3 &up_) {
    glm::vec3 up = pickSafeUpVector(direction, up_);
    glm::vec3 eye = target - glm::normalize(direction) * distance;
    // add 'direction' to 'target' to make it work for distance = 0
    viewMatrix = glm::lookAt(eye, target + direction, up);
}

void SimpleShadowCaster::lookAt(const glm::vec3 &target, float azimuth, float elevation, float distance, const glm::vec3 &up) {
    glm::vec3 direction = glm::vec3{
        glm::sin(azimuth) * glm::cos(elevation),
        glm::sin(elevation),
        glm::cos(azimuth) * glm::cos(elevation),
    };
    lookAt(target, -direction, distance, up);
}

ShadowCascade::ShadowCascade(const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution, int count) {
    mCascades.reserve(count);
    for (size_t i = 0; i < count; i++) {
        mCascades.emplace_back(device, allocator, resolution);
    }
}

void ShadowCascade::update(float frustum_fov, float frustum_aspect, glm::mat4 view_matrix, glm::vec3 light_dir) {

    float near_clip = 0.1f;
    float far_clip = distance;
    float clip_range = far_clip - near_clip;

    // cannot use camera projection matrix directly because it has an infinite far plane
    glm::mat4 camera_projection = glm::perspective(frustum_fov, frustum_aspect, near_clip, far_clip);
    glm::mat4 camera_inverse = glm::inverse(camera_projection * view_matrix);

    size_t count = mCascades.size();
    float last_split_dist = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float f = static_cast<float>(i + 1) / static_cast<float>(count);
        float split = calculateSplitDistance(lambda, near_clip, far_clip, clip_range, f);

        glm::vec3 frustum_corners[] = {
            glm::vec3(-1.0f, 1.0f, 0.0f),  glm::vec3(1.0f, 1.0f, 0.0f),  glm::vec3(1.0f, -1.0f, 0.0f),
            glm::vec3(-1.0f, -1.0f, 0.0f), glm::vec3(-1.0f, 1.0f, 1.0f),  glm::vec3(1.0f, 1.0f, 1.0f),
            glm::vec3(1.0f, -1.0f, 1.0f),   glm::vec3(-1.0f, -1.0f, 1.0f),
        };

        // Project frustum corners into world space
        for (size_t j = 0; j < 8; j++) {
            glm::vec4 p = camera_inverse * glm::vec4(frustum_corners[j], 1.0f);
            frustum_corners[j] = glm::vec3(p.x / p.w, p.y / p.w, p.z / p.w);
        }

        // Set start and end distance
        for (size_t j = 0; j < 4; j++) {
            glm::vec3 dist = frustum_corners[j + 4] - frustum_corners[j];
            frustum_corners[j + 4] = frustum_corners[j] + dist * split;
            frustum_corners[j] = frustum_corners[j] + dist * last_split_dist;
        }

        glm::vec3 frustum_center(0.0);
        for (int j = 0; j < 8; j++) {
            frustum_center += frustum_corners[j];
        }
        frustum_center /= 8.0f;

        float radius = 0.0f;
        for (int j = 0; j < 8; j++) {
            float d = glm::distance(frustum_corners[j], frustum_center);
            radius = std::max(radius, d);
        }
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // Texel alignment
        glm::mat4 light_view_mat = createTexelAlignedViewMatrix(
                light_dir, mCascades[i].resolution(), radius, frustum_center
        );

        // swap near and far plane for reverse z
        glm::vec3 max_extents = glm::vec3(radius);
        glm::vec3 min_extents = -max_extents;
        glm::mat4 light_ortho_mat = glm::ortho(
                min_extents.x, max_extents.x, min_extents.y, max_extents.y, 1000.0f, -1000.0f
        );

        mCascades[i].distance = split * clip_range * 2.0f;
        mCascades[i].viewMatrix = light_view_mat;
        mCascades[i].projectionMatrix = light_ortho_mat;
        last_split_dist = split;
    }
}

float ShadowCascade::calculateSplitDistance(float lambda, float near_clip, float far_clip, float clip_range, float f) {
    // From https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
    float clip_ratio = far_clip / near_clip;
    float log = near_clip * std::pow(clip_ratio, f);
    float uniform = near_clip + clip_range * f;
    float d = lambda * (log - uniform) + uniform;
    return (d - near_clip) / clip_range;
}

glm::mat4 ShadowCascade::createTexelAlignedViewMatrix(
        glm::vec3 light_dir, uint32_t resolution, float radius, glm::vec3 frustum_center
) {
    glm::vec3 up = pickSafeUpVector(light_dir);
    glm::mat4 zero_view = glm::lookAt(glm::vec3(0.0f), -light_dir, up);

    glm::vec4 center_light_space = zero_view * glm::vec4(frustum_center, 1.0f);
    float world_space_unit = radius * 2.0f / resolution;
    // Round the X and Y coordinates to the nearest world_space_unit (texel size)
    center_light_space.x = std::round(center_light_space.x / world_space_unit) * world_space_unit;
    center_light_space.y = std::round(center_light_space.y / world_space_unit) * world_space_unit;
    // Transform the snapped center back to world space
    glm::vec3 snapped_center = glm::inverse(zero_view) * center_light_space;

    // Adjust the light view matrix to be centered on the snapped position
    return glm::lookAt(snapped_center - light_dir, snapped_center, up);
}