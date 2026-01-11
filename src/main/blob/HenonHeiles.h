#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <vector>


class HenonHeiles {
public:
    struct Point {
        glm::vec3 position;
        glm::vec3 velocity;
        glm::vec3 acceleration;
    };

    float lambda = 1.0f;
    float mu = 1.0f;

    std::vector<Point> points;

    float boundaryRadius = 0.8f;
    float containmentStrength = 200.0f;

    explicit HenonHeiles(int count);

    void update(float dt);

private:

    [[nodiscard]] glm::vec3 calculateForce(const glm::vec3 &p) const;
};
