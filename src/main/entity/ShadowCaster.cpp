#include "ShadowCaster.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "../debug/Annotation.h"


ShadowCaster::ShadowCaster(
        const vk::Device &device, const vma::Allocator &allocator, uint32_t resolution, float dimension, float start, float end
)
    : dimension(dimension), start(start), end(end), mResolution(resolution) {
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
    // clang-format off
}

glm::mat4 ShadowCaster::projectionMatrix() const{
    float half_extent = 0.5f * dimension;
    // swap near and far plane for reverse z
    return glm::ortho(-half_extent, half_extent, -half_extent, half_extent, end, start);
}

void ShadowCaster::lookAt(const glm::vec3 &target, const glm::vec3 &direction, float distance, const glm::vec3 &up_) {
    glm::vec3 up = up_;
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

    glm::vec3 eye = target - glm::normalize(direction) * distance;
    // add 'direction' to 'target' to make it work for distance = 0
    mViewMatrix = glm::lookAt(eye, target + direction, up);
}

void ShadowCaster::lookAt(const glm::vec3 &target, float azimuth, float elevation, float distance, const glm::vec3 &up) {
    glm::vec3 direction = glm::vec3{
        glm::sin(azimuth) * glm::cos(elevation),
        glm::sin(elevation),
        glm::cos(azimuth) * glm::cos(elevation),
    };
    lookAt(target, -direction, distance, up);
}