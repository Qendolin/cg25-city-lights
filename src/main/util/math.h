#pragma once
#include <glm/common.hpp>
#include <glm/vec3.hpp>

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
    void extend(const BoundingBox& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }
};