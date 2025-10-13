#include "Camera.h"

#include "../util/math.h"

Camera::Camera(float fov, float near_plane, glm::vec3 position, glm::vec3 angles) {
    this->mFov = fov;
    this->mNearPlane = near_plane;
    this->position = position;
    this->angles = angles;

    updateProjectionMatrix();
    updateViewMatrix();
}

Camera::~Camera() = default;

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
