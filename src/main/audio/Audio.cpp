#include "Audio.h"

#include "AudioSystem.h"

std::unique_ptr<Music> Audio::createMusic(const std::string& filename) const {
    return std::make_unique<Music>(*musicBus, filename);
}

std::unique_ptr<Sound> Audio::createSound(const std::string& filename) const {
    return std::make_unique<Sound>(*soundBus, filename);
}

void Audio::update(glm::vec3 listener_position, glm::vec3 listener_direction) {
    // Can also set bus volume settings here
    system->update(listener_position, listener_direction);
}