#include "Camera.h"

#include "../util/math.h"

Camera::Camera(float fov, float near_plane, glm::vec3 position, glm::vec3 angles)
    : mFov{fov}, mNearPlane{near_plane}, position{position}, angles{angles} {
    updateProjectionMatrix();
    updateViewMatrix();
}

Camera::Camera(float fov, float near_plane, glm::mat4 camera_instance_transform) : mFov{fov}, mNearPlane{near_plane} {
    updateBasedOnTransform(camera_instance_transform);
    updateProjectionMatrix();
}

Camera::~Camera() = default;

void Camera::updateBasedOnTransform(const glm::mat4 &camera_instance_transform) {
    position = glm::vec3(camera_instance_transform[3]);

    float z, y, x;
    glm::extractEulerAngleZYX(camera_instance_transform, z, y, x);
    angles = glm::vec3(x, y, z);

    mViewMatrix = glm::inverse(camera_instance_transform);
}

void Camera::updateProjectionMatrix() {
    float a = mViewportSize.x / mViewportSize.y;
    mAspect = a;
    mProjectionMatrix = util::createReverseZInfiniteProjectionMatrix(mViewportSize, mFov, mNearPlane);
}

void Camera::updateViewMatrix() {
    mViewMatrix = glm::mat4(1.0f);
    mViewMatrix = glm::translate(mViewMatrix, position);
    mViewMatrix = glm::rotate(mViewMatrix, angles.z, {0, 0, 1});
    mViewMatrix = glm::rotate(mViewMatrix, angles.y, {0, 1, 0});
    mViewMatrix = glm::rotate(mViewMatrix, angles.x, {1, 0, 0});
    mViewMatrix = glm::inverse(mViewMatrix);
}
