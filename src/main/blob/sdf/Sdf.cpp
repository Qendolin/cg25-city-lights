#include "Sdf.h"

namespace blob {

    float Sdf::sphere(glm::vec3 point, float size) { return glm::length(point) - size; }

    float Sdf::smoothMin(float a, float b, float smoothing) {
        float h = glm::clamp(0.5 + 0.5 * (b - a) / smoothing, 0.0, 1.0);
        return glm::mix(b, a, h) - smoothing * h * (1.0 - h);
    }

    float Sdf::smoothMax(float a, float b, float smoothing) { return -smoothMin(-a, -b, smoothing); }

    float Sdf::smoothSub(float a, float b, float smoothing) { return smoothMax(a, -b, smoothing); }

} // namespace blob
