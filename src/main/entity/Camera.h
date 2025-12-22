#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

/// <summary>
/// Represents a camera in a 3D scene.
/// </summary>
class Camera {
    /// <summary>
    /// Vertical field of view, in radians.
    /// </summary>
    float mFov;
    /// <summary>
    /// Aspect ratio (width / height).
    /// </summary>
    float mAspect = 1.0;
    /// <summary>
    /// The size of the viewport in pixels.
    /// </summary>
    glm::vec2 mViewportSize = {1600, 900};
    /// <summary>
    /// Distance of the near clipping plane.
    /// </summary>
    float mNearPlane;

    glm::mat4 mViewMatrix = glm::mat4(1.0);
    glm::mat4 mProjectionMatrix = glm::mat4(1.0);

    /// <summary>
    /// Recalculates the projection matrix.
    /// </summary>
    void updateProjectionMatrix();

public:
    /// <summary>
    /// The position of the camera in world space.
    /// </summary>
    glm::vec3 position{};
    /// <summary>
    /// The orientation of the camera as pitch, yaw, and roll, in radians.
    /// </summary>
    glm::vec3 angles{};

    /// <summary>
    /// Initializes a new instance of the Camera class.
    /// <param name="fov">Vertical field of view, in radians.</param>
    /// <param name="near_plane">Distance of the near plane.</param>
    /// <param name="position">Position of the camera.</param>
    /// <param name="angles">Orientation of the camera (pitch, yaw, roll) in radians.</param>
    /// </summary>
    Camera(float fov, float near_plane, glm::vec3 position, glm::vec3 angles);

    /// <summary>
    /// Initializes a new instance of the Camera class.
    /// <param name="fov">Vertical field of view, in radians.</param>
    /// <param name="near_plane">Distance of the near plane.</param>
    /// <param name="view_matrix">View matrix that encodes the position and orientation.</param>
    /// </summary>
    Camera(float fov, float near_plane, glm::mat4 view_matrix);

    ~Camera();

    /// <summary>
    /// Sets the viewport size and updates the projection matrix.
    /// </summary>
    /// <param name="width">Width of the viewport area, in pixels.</param>
    /// <param name="height">Height of the viewport area, in pixels.</param>
    void setViewport(float width, float height) {
        if (width == mViewportSize.x && height == mViewportSize.y)
            return;
        mViewportSize = {width, height};
        updateProjectionMatrix();
    }

    /// <summary>
    /// Sets the near plane distance and updates the projection matrix.
    /// </summary>
    /// <param name="near_plane">Distance of the near plane.</param>
    void setNearPlane(float near_plane) {
        if (near_plane == mNearPlane)
            return;
        mNearPlane = near_plane;
        updateProjectionMatrix();
    }

    /// <summary>
    /// Sets the vertical field of view and updates the projection matrix.
    /// </summary>
    /// <param name="fov">Vertical field of view, in radians.</param>
    void setFov(float fov) {
        if (fov == mFov)
            return;
        mFov = fov;
        updateProjectionMatrix();
    }

    /// <summary>
    /// Recalculates the view matrix based on the camera's position and orientation.
    /// </summary>
    void updateViewMatrix();

    /// <summary>
    /// Updates the view matrix and position and angles according to the transform matrix.
    /// </summary>
    /// <param name="camera_instance_transform">The transform matrix of the camera in world space.
    /// Expected to be free of scaling and shearing.</param>
    void updateBasedOnTransform(const glm::mat4 &camera_instance_transform);

    /// <summary>
    /// Gets the distance of the near clipping plane.
    /// </summary>
    /// <returns>The distance of the near plane.</returns>
    [[nodiscard]] float nearPlane() const { return mNearPlane; }

    /// <summary>
    /// Gets the vertical field of view.
    /// </summary>
    /// <returns>The vertical FOV in radians.</returns>
    [[nodiscard]] float fov() const { return mFov; }

    /// <summary>
    /// Gets the aspect ratio of the camera frustum.
    /// </summary>
    /// <returns>The frustum aspect ratio (width / height).</returns>
    [[nodiscard]] float aspect() const { return mAspect; }

    /// <summary>
    /// Gets the projection matrix of the camera.
    /// </summary>
    /// <returns>The projection matrix.</returns>
    [[nodiscard]] glm::mat4 projectionMatrix() const { return mProjectionMatrix; }

    /// <summary>
    /// Gets the view matrix of the camera.
    /// </summary>
    /// <returns>The view matrix.</returns>
    [[nodiscard]] glm::mat4 viewMatrix() const { return mViewMatrix; }

    /// <summary>
    /// Gets the rotation matrix of the camera.
    /// </summary>
    /// <returns>The 3x3 rotation matrix.</returns>
    [[nodiscard]] glm::mat3 rotationMatrix() const { return glm::transpose(glm::mat3(mViewMatrix)); }

    /// <summary>
    /// Gets the combined view-projection matrix.
    /// </summary>
    /// <returns>The view-projection matrix.</returns>
    [[nodiscard]] glm::mat4 viewProjectionMatrix() const { return mProjectionMatrix * mViewMatrix; }
};
