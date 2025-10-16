#pragma once
#include <glm/common.hpp>
#include <glm/vec3.hpp>

namespace util {

    /// <summary>
    /// Represents an axis-aligned bounding box.
    /// </summary>
    struct BoundingBox {
        /// <summary>
        /// The minimum corner of the bounding box.
        /// </summary>
        glm::vec3 min = glm::vec3{std::numeric_limits<float>::infinity()};
        /// <summary>
        /// The maximum corner of the bounding box.
        /// </summary>
        glm::vec3 max = glm::vec3{-std::numeric_limits<float>::infinity()};

        /// <summary>
        /// Extends the bounding box to include the given point.
        /// </summary>
        void extend(const glm::vec3 &p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        /// <summary>
        /// Extends the bounding box to include the other bounding box.
        /// </summary>
        void extend(const BoundingBox &other) {
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }
    };

    inline glm::mat4 createReverseZInfiniteProjectionMatrix(float aspect_ratio, float fov, float near_plane) {
        float f = 1.0f / std::tan(fov / 2.0f);
        // This is a reversed projection matrix with an infinite far plane.
        // clang-format off
        return {
            f / aspect_ratio, 0.0f, 0.0f, 0.0f,
            0.0f, f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, -1.0f,
            0.0f, 0.0f, near_plane, 0.0f};
        // clang-format on
    }

    inline glm::mat4 createReverseZInfiniteProjectionMatrix(glm::vec2 viewport_size, float fov, float near_plane) {
        float a = viewport_size.x / viewport_size.y;
        return createReverseZInfiniteProjectionMatrix(a, fov, near_plane);
    }
} // namespace util
