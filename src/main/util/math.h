#pragma once
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

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

    inline int32_t divCeil(int32_t x, int32_t y) {
        return (x + y - 1) / y;
    }

    inline uint32_t divCeil(uint32_t x, uint32_t y) {
        return (x + y - 1) / y;
    }

    inline int32_t nextLowestPowerOfTwo(int32_t n) {
        if (n <= 0) return 0;  // No power of two for zero or negative numbers

        uint32_t u = static_cast<uint32_t>(n);
        u |= (u >> 1);
        u |= (u >> 2);
        u |= (u >> 4);
        u |= (u >> 8);
        u |= (u >> 16);

        return static_cast<int32_t>(u - (u >> 1));
    }

    inline uint32_t nextLowestPowerOfTwo(uint32_t n) {
        if (n == 0) return 0;

        // Shift right until only the highest bit remains
        n |= (n >> 1);
        n |= (n >> 2);
        n |= (n >> 4);
        n |= (n >> 8);
        n |= (n >> 16);

        return n - (n >> 1);
    }

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
