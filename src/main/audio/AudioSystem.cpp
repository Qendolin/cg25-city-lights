#include <soloud.h>
#include <soloud_wav.h>
#include <soloud_wavstream.h>
#undef min
#undef max

#include <utility>

#include "../util/Logger.h"
#include "Audio.h"

AudioSystem::AudioSystem() {
    mSoloud = new SoLoud::Soloud();
    mSoloud->init();
}

AudioSystem::~AudioSystem() {
    mSoloud->deinit();
    delete mSoloud;
}

void AudioSystem::update(glm::vec3 position, glm::vec3 direction) {
    mSoloud->set3dListenerPosition(position.x, position.y, position.z);
    mSoloud->set3dListenerAt(direction.x, direction.y, direction.z);
    mSoloud->update3dAudio();
}

void AudioSystem::setVolume(float volume) {
    if (volume == mVolume)
        return;
    mVolume = volume;
    mSoloud->setGlobalVolume(volume);
}

AudioBus::AudioBus(AudioSystem &system) : system(system) {
    mBus = new SoLoud::Bus();
    mHandle = system.mSoloud->play(*mBus);
}

AudioBus::~AudioBus() {
    mBus->stop();
    delete mBus;
}

bool AudioBus::isPlaying() const { return system.mSoloud->isValidVoiceHandle(mHandle); }

void AudioBus::play() {
    if (system.mSoloud->isValidVoiceHandle(mHandle))
        return;
    mHandle = system.mSoloud->play(*mBus);
}

void AudioBus::stop() {
    if (mHandle == 0)
        return;
    mBus->stop();
    mHandle = 0;
}

void AudioBus::fadeFilterParam(int filter, int attribute, float value, float time_sec) {
    system.mSoloud->fadeFilterParameter(mHandle, filter, attribute, value, time_sec);
}

void AudioBus::setFilterParam(int filter, int attribute, float value) {
    system.mSoloud->setFilterParameter(mHandle, filter, attribute, value);
}

void AudioBus::setVolume(float volume) { system.mSoloud->setVolume(mHandle, volume); }

Music::Music(AudioBus &bus, const std::string& filename) : bus(bus) {
    mWav = new SoLoud::WavStream();
    auto result = mWav->load(filename.c_str());
    if (result != 0) {
        Logger::fatal("Failed to load music from '" + filename + "'");
    }
    mWav->setSingleInstance(true);
}

Music::Music(Music &&other) noexcept
    : mWav(std::exchange(other.mWav, nullptr)),
      mHandle(std::exchange(other.mHandle, 0)),
      mPaused(other.mPaused),
      mLooping(other.mLooping),
      mVolume(other.mVolume),
      mSpeed(other.mSpeed),
      bus(other.bus) {}

Music::~Music() {
    if (mWav != nullptr)
        mWav->stop();
    delete mWav;
}

float Music::getVolume() const { return mVolume; }

void Music::setVolume(float volume) {
    mVolume = volume;
    if (mHandle == 0)
        return;

    bus.system.mSoloud->setVolume(mHandle, volume);
}

void Music::setPan(float pan) {
    mPan = pan;
    if (mHandle == 0)
        return;

    bus.system.mSoloud->setPan(mHandle, pan);
}

void Music::setSpeed(float speed) {
    if (speed <= 0.0001f) {
        Logger::warning("cannot set the speed to 0 or less");
        speed = 0.0001f;
    }

    mSpeed = speed;
    if (mHandle == 0)
        return;

    bus.system.mSoloud->setRelativePlaySpeed(mHandle, speed);
}

void Music::play() {
    if (mHandle != 0 && mPaused) {
        setPaused(false);
    } else if (mHandle == 0) {
        mHandle = bus.mBus->play(*mWav, mVolume, mPan, false);
        bus.system.mSoloud->setLooping(mHandle, mLooping);
        bus.system.mSoloud->setRelativePlaySpeed(mHandle, mSpeed);
        bus.system.mSoloud->setProtectVoice(mHandle, true);
        mPaused = false;
    }
}

void Music::pause() { setPaused(true); }

void Music::setPaused(bool pause) {
    mPaused = pause;
    if (mHandle == 0)
        return;
    bus.system.mSoloud->setPause(mHandle, pause);
}

void Music::stop() {
    if (mHandle == 0)
        return;
    bus.system.mSoloud->stop(mHandle);
    mHandle = 0;
}

bool Music::isPlaying() const {
    if (mHandle == 0)
        return false;

    return !mPaused && bus.system.mSoloud->isValidVoiceHandle(mHandle);
}

void Music::setLooping(bool looping) {
    mLooping = looping;
    if (mHandle == 0)
        return;

    bus.system.mSoloud->setLooping(mHandle, looping);
}

bool Music::isLooping() const { return mLooping; }

double Music::duration() const { return mWav->getLength(); }

void Music::seek(double seconds) { bus.system.mSoloud->seek(mHandle, seconds); }

Sound::Sound(AudioBus &bus, const std::string& filename) : bus(bus) {
    mWav = new SoLoud::Wav();
    auto result = mWav->load(filename.c_str());
    if (result != 0) {
        Logger::fatal("Failed to load sound effect from '" + filename + "'");
    }
    mWav->set3dAttenuation(SoLoud::AudioSource::INVERSE_DISTANCE, 0.2f);
    mWav->set3dDistanceDelay(false);
}

Sound::Sound(Sound &&other) noexcept : mWav(std::exchange(other.mWav, nullptr)), bus(other.bus) {}

Sound::~Sound() {
    if (mWav != nullptr)
        mWav->stop();
    delete mWav;
}

void Sound::stop() { mWav->stop(); }

void Sound::setVolume(float volume) { mWav->setVolume(volume); }

void Sound::setLooping(bool looping) { mWav->setLooping(looping); }

void Sound::setLoopPoint(double point) { mWav->setLoopPoint(point); }

void Sound::set3dListenerRelative(bool relative) { mWav->set3dListenerRelative(relative); }

void Sound::set3dMinMaxDistance(float min, float max) { mWav->set3dMinMaxDistance(min, max); }

void Sound::set3dDopplerFactor(float factor) { mWav->set3dDopplerFactor(factor); }

double Sound::duration() const { return mWav->getLength(); }

SoundInstance3d Sound::play3dEvent(glm::vec3 position, float volume, glm::vec3 velocity) {
    auto handle = bus.mBus->play3d(*mWav, position.x, position.y, position.z, velocity.x, velocity.y, velocity.y, volume);
    return {*this, handle, false, volume, position, velocity};
}

SoundInstance2d Sound::play2dEvent(float volume, float pan) {
    auto handle = bus.mBus->play(*mWav, volume, pan);
    return {*this, handle, false, volume, pan};
}

SoundInstance3d *Sound::play3d(glm::vec3 position, float volume, glm::vec3 velocity) {
    auto handle = bus.mBus->play3d(*mWav, position.x, position.y, position.z, velocity.x, velocity.y, velocity.y, volume);
    return new SoundInstance3d(*this, handle, true, volume, position, velocity);
}

SoundInstance2d *Sound::play2d(float volume, float pan) {
    auto handle = bus.mBus->play(*mWav, volume, pan);
    return new SoundInstance2d(*this, handle, true, volume, pan);
}

SoundInstance::SoundInstance(Sound &sound, unsigned int handle, bool lifetime_bound, float volume)
    : mHandle(handle), mLifetimeBound(lifetime_bound), mVolume(volume), sound(sound) {}

SoundInstance::~SoundInstance() {
    if (mLifetimeBound) {
        sound.bus.system.mSoloud->stop(mHandle);
    }
}

float SoundInstance::getVolume() const { return mVolume; }

void SoundInstance::setVolume(float volume) {
    mVolume = volume;
    if (mHandle == 0)
        return;

    sound.bus.system.mSoloud->setVolume(mHandle, volume);
}

void SoundInstance::setSpeed(float speed) {
    if (speed <= 0.0001f) {
        Logger::warning("cannot set the speed to 0 or less");
        speed = 0.0001f;
    }

    mSpeed = speed;
    if (mHandle == 0)
        return;

    sound.bus.system.mSoloud->setRelativePlaySpeed(mHandle, speed);
}

void SoundInstance::play() { setPaused(false); }

void SoundInstance::pause() { setPaused(true); }

void SoundInstance::setPaused(bool pause) {
    mPaused = pause;
    if (mHandle == 0)
        return;
    sound.bus.system.mSoloud->setPause(mHandle, pause);
}

void SoundInstance::stop() {
    if (mHandle == 0)
        return;
    sound.bus.system.mSoloud->stop(mHandle);
    mHandle = 0;
}

bool SoundInstance::isPlaying() const {
    if (mHandle == 0)
        return false;

    return !mPaused && sound.bus.system.mSoloud->isValidVoiceHandle(mHandle);
}

void SoundInstance::setLooping(bool looping) {
    mLooping = looping;
    if (mHandle == 0)
        return;

    sound.bus.system.mSoloud->setLooping(mHandle, looping);
}

bool SoundInstance::isLooping() const { return mLooping; }

void SoundInstance::seek(double seconds) { sound.bus.system.mSoloud->seek(mHandle, seconds); }

SoundInstance3d::SoundInstance3d(
        Sound &sound, unsigned int handle, bool lifetime_bound, float volume, glm::vec3 position, glm::vec3 velocity
)
    : SoundInstance(sound, handle, lifetime_bound, volume), mPosition(position), mVelocity(velocity) {}

void SoundInstance3d::setPosition(glm::vec3 position) {
    mPosition = position;
    if (mHandle == 0)
        return;

    sound.bus.system.mSoloud->set3dSourcePosition(mHandle, position.x, position.y, position.z);
}

glm::vec3 SoundInstance3d::getPosition() const { return mPosition; }

void SoundInstance3d::setVelocity(glm::vec3 velocity) {
    mVelocity = velocity;
    if (mHandle == 0)
        return;

    sound.bus.system.mSoloud->set3dSourceVelocity(mHandle, velocity.x, velocity.y, velocity.z);
}

glm::vec3 SoundInstance3d::getVelocity() const { return mVelocity; }

void SoundInstance3d::setPositionVelocity(glm::vec3 position, glm::vec3 velocity) {
    mVelocity = velocity;
    mPosition = position;
    if (mHandle == 0)
        return;

    sound.bus.system.mSoloud->set3dSourceParameters(
            mHandle, position.x, position.y, position.z, velocity.x, velocity.y, velocity.z
    );
}

SoundInstance2d::SoundInstance2d(Sound &sound, unsigned int handle, bool lifetime_bound, float volume, float pan)
    : SoundInstance(sound, handle, lifetime_bound, volume), mPan(pan) {}

void SoundInstance2d::setPan(float pan) {
    mPan = pan;
    if (mHandle == 0)
        return;

    sound.bus.system.mSoloud->setPan(mHandle, pan);
}

float SoundInstance2d::getPan() const { return mPan; }
