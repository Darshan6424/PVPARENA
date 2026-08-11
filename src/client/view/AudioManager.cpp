#include "client/view/AudioManager.h"

namespace {
// Rough mix levels, tuned by ear.
constexpr float kVolumes[] = {
    85.f,   // AttackSwing
    100.f,  // ParrySuccess
    90.f,   // HitLand
    80.f,   // BlockHit
    70.f,   // UiClick
    100.f,  // MatchEnd
};
static_assert(sizeof(kVolumes) / sizeof(kVolumes[0]) ==
              static_cast<std::size_t>(Sfx::Count), "volume table out of sync");
}

void SoundPool::init(const sf::SoundBuffer& buffer, std::size_t size) {
    sounds_.clear();
    sounds_.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        sounds_.emplace_back(buffer);
    }
    next_ = 0;
}

void SoundPool::play(float volume) {
    if (sounds_.empty()) return;
    sf::Sound& s = sounds_[next_];
    next_ = (next_ + 1) % sounds_.size();
    s.setVolume(volume);
    s.play();
}

void AudioManager::init() {
    for (std::size_t i = 0; i < pools_.size(); ++i) {
        Sfx id = static_cast<Sfx>(i);
        if (assets_.has(id)) pools_[i].init(assets_.sound(id));
    }
}

void AudioManager::play(Sfx id) {
    std::size_t i = static_cast<std::size_t>(id);
    pools_[i].play(kVolumes[i]);
}

void AudioManager::playMusic(Track track) {
    int wanted = static_cast<int>(track);
    if (currentTrack_ == wanted) return;

    stopMusic();
    if (assets_.has(track)) {
        assets_.music(track).play();
    }
    currentTrack_ = wanted;
}

void AudioManager::stopMusic() {
    for (std::size_t i = 0; i < static_cast<std::size_t>(Track::Count); ++i) {
        Track t = static_cast<Track>(i);
        if (assets_.has(t)) assets_.music(t).stop();
    }
    currentTrack_ = -1;
}
