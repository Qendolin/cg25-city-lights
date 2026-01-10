#pragma once
#include <array>
#include <glm/common.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/matrix_operation.hpp>
#include <glm/gtx/quaternion.hpp>
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

    inline int32_t divCeil(int32_t x, int32_t y) { return (x + y - 1) / y; }

    inline uint32_t divCeil(uint32_t x, uint32_t y) { return (x + y - 1) / y; }

    inline int32_t nextLowestPowerOfTwo(int32_t n) {
        if (n <= 0)
            return 0; // No power of two for zero or negative numbers

        auto u = static_cast<uint32_t>(n);
        u |= (u >> 1);
        u |= (u >> 2);
        u |= (u >> 4);
        u |= (u >> 8);
        u |= (u >> 16);

        return static_cast<int32_t>(u - (u >> 1));
    }

    inline uint32_t nextLowestPowerOfTwo(uint32_t n) {
        if (n == 0)
            return 0;

        // Shift right until only the highest bit remains
        n |= (n >> 1);
        n |= (n >> 2);
        n |= (n >> 4);
        n |= (n >> 8);
        n |= (n >> 16);

        return n - (n >> 1);
    }

    inline size_t alignOffset(size_t offset, size_t alignment) { return (offset + alignment - 1) & ~(alignment - 1); }

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

    // Returns the six frustum planes from a combined projection-view matrix.
    // Planes are returned as glm::vec4: (a, b, c, d), where plane equation is ax + by + cz + d = 0
    // Order of planes: left, right, bottom, top, near, far
    inline std::array<glm::vec4, 6> extractFrustumPlanes(const glm::mat4 &mat, bool normalize = true) {
        std::array<glm::vec4, 6> planes = {};

        // clang-format off
        // Left plane
        planes[0] = glm::vec4(
            mat[0][3] + mat[0][0],
            mat[1][3] + mat[1][0],
            mat[2][3] + mat[2][0],
            mat[3][3] + mat[3][0]
        );

        // Right plane
        planes[1] = glm::vec4(
            mat[0][3] - mat[0][0],
            mat[1][3] - mat[1][0],
            mat[2][3] - mat[2][0],
            mat[3][3] - mat[3][0]
        );

        // Bottom plane
        planes[2] = glm::vec4(
            mat[0][3] + mat[0][1],
            mat[1][3] + mat[1][1],
            mat[2][3] + mat[2][1],
            mat[3][3] + mat[3][1]
        );

        // Top plane
        planes[3] = glm::vec4(
            mat[0][3] - mat[0][1],
            mat[1][3] - mat[1][1],
            mat[2][3] - mat[2][1],
            mat[3][3] - mat[3][1]
        );

        // Near plane
        planes[4] = glm::vec4(
            mat[0][3] + mat[0][2],
            mat[1][3] + mat[1][2],
            mat[2][3] + mat[2][2],
            mat[3][3] + mat[3][2]
        );

        // Far plane
        planes[5] = glm::vec4(
            mat[0][3] - mat[0][2],
            mat[1][3] - mat[1][2],
            mat[2][3] - mat[2][2],
            mat[3][3] - mat[3][2]
        );
        // clang-format on

        // Optional: normalize planes
        if (normalize) {
            for (auto &plane: planes) {
                float length = glm::length(glm::vec3(plane));
                plane /= length;
            }
        }

        return planes;
    }

    inline glm::vec2 octahedronEncode(const glm::vec3 &n_in) {
        glm::vec3 n = glm::normalize(n_in);

        // Project onto octahedron
        float invL1 = 1.0f / (std::abs(n.x) + std::abs(n.y) + std::abs(n.z));
        n *= invL1;

        // Branchless fold for the lower hemisphere
        glm::vec2 fold = (glm::vec2(1.0f) - glm::abs(glm::vec2(n.y, n.x))) * glm::sign(glm::vec2(n.x, n.y));

        glm::vec2 xy = glm::mix(glm::vec2(n.x, n.y), fold, n.z <= 0.0f ? 1.0f : 0.0f);

        return xy * 0.5f + 0.5f; // Map to [0,1]
    }


    inline glm::vec3 octahedronDecode(const glm::vec2 &f) {
        // Back to [-1,1]
        glm::vec2 n = f * 2.0f - 1.0f;

        glm::vec3 v(n.x, n.y, 1.0f - std::abs(n.x) - std::abs(n.y));

        // Unfold
        float t = glm::max(-v.z, 0.0f);
        v.x += (v.x >= 0.0f ? 1.0f : -1.0f) * t;
        v.y += (v.y >= 0.0f ? 1.0f : -1.0f) * t;

        return glm::normalize(v);
    }

    inline glm::vec3 safeUpVector(const glm::vec3 &direction, glm::vec3 up = {0.0f, 1.0f, 0.0f}) {
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


    inline void decomposeTransform(const glm::mat4 &transform, glm::vec3 *translation, glm::quat *rotation, glm::vec3 *scale) {
        // https://math.stackexchange.com/a/1463487/1014081
        // calculate scale
        *scale = {glm::length(transform[0]), glm::length(transform[1]), glm::length(transform[2])};
        // calculate rotaton
        glm::mat3 rotation_mat = transform * glm::diagonal4x4(glm::vec4(1.0 / scale->x, 1.0 / scale->y, 1.0 / scale->z, 1.0));

        *translation = transform[3];
        *rotation = glm::quat_cast(rotation_mat);
    }

} // namespace util
