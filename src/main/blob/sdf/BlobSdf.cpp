#include "BlobSdf.h"

#include <cmath>

void BlobSdf::advanceTime(float dt) {
    time += dt;
    time -= std::floor(time);
}

float BlobSdf::value(glm::vec3 point) const {
    float x = time;

    glm::vec3 offset1 = {.0f, 0.2f + 1.3 * x, 0.f};

    // Core sphere
    float s0 = sphere(point, 0.5);

    // TODO: Multiple Spheres rotating around core sphere to simulate blobbiness
    
    // Moving sphere - simulates dripping effect
    float s1 = sphere(point - offset1, 0.2);

    float val = smoothMin(s0, s1, 0.4);

    // Simulate ground - assuming the sdf sampling volume ends at y = 1
    return smoothMin(val, 1.02f - point.y, 0.2);
}
