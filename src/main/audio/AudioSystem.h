#pragma once

#include <glm/glm.hpp>
#include <string>

// Reference: https://solhsa.com/soloud/

namespace SoLoud {
    class Soloud;
    class Bus;
    class Wav;
    class WavStream;
    typedef unsigned int handle;
} // namespace SoLoud

class SoundInstance;
class SoundInstance3d;
class SoundInstance2d;

class AudioSystem {
    friend class AudioBus;
    friend class Music;
    friend class Sound;
    friend class SoundInstance;
    friend class SoundInstance2d;
    friend class SoundInstance3d;

private:
    SoLoud::Soloud *mSoloud;
    float mVolume = 1;

public:
    AudioSystem();

    ~AudioSystem();

    void update(glm::vec3 position, glm::vec3 direction);

    void setVolume(float volume);
};

class AudioBus {
    friend class Music;
    friend class Sound;

    SoLoud::Bus *mBus;
    SoLoud::handle mHandle = 0;

public:
    AudioSystem &system;

    explicit AudioBus(AudioSystem &system);

    ~AudioBus();

    [[nodiscard]] bool isPlaying() const;

    void play();

    void stop();

    void fadeFilterParam(int filter, int attribute, float value, float time_sec);

    void setFilterParam(int filter, int attribute, float value);

    void setVolume(float volume);
};

class Music {
    SoLoud::WavStream *mWav;
    SoLoud::handle mHandle = 0;

    bool mPaused = false;
    bool mLooping = false;
    float mVolume = 1;
    float mSpeed = 1;
    float mPan = 0;

public:
    AudioBus &bus;
    Music(AudioBus &bus, const std::string &filename);

    // prevent copy
    Music(Music const &) = delete;
    Music &operator=(Music const &) = delete;

    // allow move
    Music(Music &&other) noexcept;

    ~Music();

    [[nodiscard]] float getVolume() const;

    void setVolume(float volume);

    void setPan(float pan);

    void setSpeed(float speed);

    void play();

    void pause();

    void setPaused(bool pause);

    void stop();

    [[nodiscard]] bool isPlaying() const;

    void setLooping(bool looping);

    [[nodiscard]] bool isLooping() const;

    [[nodiscard]] double duration() const;

    void seek(double seconds);
};

class Sound {
    SoLoud::Wav *mWav;

public:
    AudioBus &bus;
    Sound(AudioBus &bus, const std::string &filename);

    // prevent copy
    Sound(Sound const &) = delete;
    Sound &operator=(Sound const &) = delete;

    // allow move
    Sound(Sound &&other) noexcept;

    ~Sound();

    void stop();

    void setVolume(float volume);

    void setLooping(bool looping);

    void setLoopPoint(double point);

    void set3dListenerRelative(bool relative);

    void set3dMinMaxDistance(float min, float max);

    void set3dDopplerFactor(float factor);

    [[nodiscard]] double duration() const;

    SoundInstance3d play3dEvent(glm::vec3 position, float volume, glm::vec3 velocity = glm::vec3{0, 0, 0});

    SoundInstance2d play2dEvent(float volume, float pan = 0.0f);

    SoundInstance3d *play3d(glm::vec3 position, float volume, glm::vec3 velocity = glm::vec3{0, 0, 0});

    SoundInstance2d *play2d(float volume, float pan = 0.0f);
};

class SoundInstance {
protected:
    SoLoud::handle mHandle = 0;
    bool mLifetimeBound = true;

    bool mPaused = false;
    bool mLooping = false;
    float mVolume = 1;
    float mSpeed = 1;

public:
    Sound &sound;
    SoundInstance(Sound &sound, unsigned int handle, bool lifetime_bound, float volume);

    // prevent copy
    SoundInstance(SoundInstance const &) = delete;
    SoundInstance &operator=(SoundInstance const &) = delete;

    virtual ~SoundInstance();

    [[nodiscard]] float getVolume() const;

    void setVolume(float volume);

    void setSpeed(float speed);

    void play();

    void pause();

    void setPaused(bool pause);

    void stop();

    [[nodiscard]] bool isPlaying() const;

    void setLooping(bool looping);

    [[nodiscard]] bool isLooping() const;

    void seek(double seconds);
};

/**
 * Note: A sound source is not actually moved by its velocity. It is all static.
 */
class SoundInstance3d : public SoundInstance {
private:
    glm::vec3 mPosition;
    glm::vec3 mVelocity;

public:
    SoundInstance3d(Sound &sound, unsigned int handle, bool lifetime_bound, float volume, glm::vec3 position, glm::vec3 veloity);

    void setPosition(glm::vec3 position);

    [[nodiscard]] glm::vec3 getPosition() const;

    void setVelocity(glm::vec3 velocity);

    [[nodiscard]] glm::vec3 getVelocity() const;

    void setPositionVelocity(glm::vec3 position, glm::vec3 velocity);
};

class SoundInstance2d : public SoundInstance {
    float mPan;

public:
    SoundInstance2d(Sound &sound, unsigned int handle, bool lifetime_bound, float volume, float pan);

    void setPan(float pan);

    [[nodiscard]] float getPan() const;
};
