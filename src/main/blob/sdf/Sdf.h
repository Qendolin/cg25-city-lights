#pragma once

#include <glm/glm.hpp>

class Sdf {
public:
    virtual float value(glm::vec3 p) const = 0;

    static float sphere(glm::vec3 point, float size);
    static float smoothMin(float a, float b, float smoothing);
    static float smoothMax(float a, float b, float smoothing);
    static float smoothSub(float a, float b, float smoothing);
};