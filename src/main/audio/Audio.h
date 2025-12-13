#pragma once

#include <memory>

#include "AudioSystem.h"

class Audio {
   public:
    std::unique_ptr<AudioSystem> system;
    std::unique_ptr<AudioBus> musicBus;
    std::unique_ptr<AudioBus> soundBus;

    Audio() {
        system = std::make_unique<AudioSystem>();
        musicBus = std::make_unique<AudioBus>(*system);
        soundBus = std::make_unique<AudioBus>(*system);
    }

    [[nodiscard]] std::unique_ptr<Music> createMusic(const std::string& filename) const;

    [[nodiscard]] std::unique_ptr<Sound> createSound(const std::string& filename) const;

    void update(glm::vec3 listener_position, glm::vec3 listener_direction);
};
