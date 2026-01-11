#include "HenonHeiles.h"

HenonHeiles::HenonHeiles(int count) {
    for (int i = 0; i < count; ++i) {
        auto t = static_cast<float>(i);

        Point &p = points.emplace_back();

        float radius = 0.2f + (fmod(t * 0.13f, 0.1f));
        p.position.x = radius * cos(t * 1.1f);
        p.position.y = radius * sin(t * 1.7f);
        p.position.z = radius * cos(t * 2.3f);

        float speed = 0.3f + (fmod(t * 0.07f, 0.1f));
        p.velocity.x = speed * sin(t * 3.5f);
        p.velocity.y = speed * cos(t * 4.1f);
        p.velocity.z = speed * sin(t * 5.7f);

        p.acceleration = calculateForce(p.position);
    }
}

void HenonHeiles::update(float dt) {
    for (auto &p: points) {
        // Velocity Verlet Integration
        glm::vec3 vHalf = p.velocity + 0.5f * p.acceleration * dt;
        p.position += vHalf * dt;

        glm::vec3 nextAcc = calculateForce(p.position);


        p.velocity = vHalf + 0.5f * nextAcc * dt;
        p.acceleration = nextAcc;
    }
}

glm::vec3 HenonHeiles::calculateForce(const glm::vec3 &p) const {
    float ax = -p.x - 2.0f * lambda * p.x * p.y;
    float ay = -p.y - lambda * (p.x * p.x - p.y * p.y) - 2.0f * mu * p.y * p.z;
    float az = -p.z - mu * (p.y * p.y - p.z * p.z);

    glm::vec3 force = glm::vec3(ax, ay, az);

    float dist = glm::length(p);
    if (dist > boundaryRadius) {
        float penetration = dist - boundaryRadius;

        glm::vec3 returnDir = -glm::normalize(p);
        force += returnDir * (penetration * penetration * containmentStrength);
    }

    return force;
}
