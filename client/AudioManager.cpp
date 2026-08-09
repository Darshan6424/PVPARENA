#include "AudioManager.h"
#include <cstdio>

void SoundPool::init(const sf::SoundBuffer& buffer, std::size_t poolSize) {
    sounds_.clear();
    sounds_.reserve(poolSize);
    for (std::size_t i = 0; i < poolSize; ++i) {
        sounds_.emplace_back(buffer);
    }
    nextIndex_ = 0;
}

void SoundPool::play(float volume) {
    if (sounds_.empty()) return;
    sf::Sound& s = sounds_[nextIndex_];
    nextIndex_ = (nextIndex_ + 1) % sounds_.size();
    s.setVolume(volume);
    s.play();
}

namespace {
    void loadSfx(sf::SoundBuffer& buffer, SoundPool& pool, const std::string& path) {
        if (buffer.loadFromFile(path)) {
            pool.init(buffer);
        } else {
            std::fprintf(stderr, "AudioManager: missing sound file %s\n", path.c_str());
        }
    }
}

void AudioManager::loadAll(const std::string& assetsDir) {
    titleMusicLoaded_ = titleMusic_.openFromFile(assetsDir + "audio/title_music.ogg");
    battleMusicLoaded_ = battleMusic_.openFromFile(assetsDir + "audio/battle_music.ogg");

    if (titleMusicLoaded_) {
        titleMusic_.setLooping(true);
        titleMusic_.setVolume(55.f);
    } else {
        std::fprintf(stderr, "AudioManager: missing title_music.ogg\n");
    }
    if (battleMusicLoaded_) {
        battleMusic_.setLooping(true);
        battleMusic_.setVolume(55.f);
    } else {
        std::fprintf(stderr, "AudioManager: missing battle_music.ogg\n");
    }

    loadSfx(swingBuffer_,    swingPool_,    assetsDir + "audio/attack_swing.ogg");
    loadSfx(parryBuffer_,    parryPool_,    assetsDir + "audio/parry_success.ogg");
    loadSfx(hitBuffer_,      hitPool_,      assetsDir + "audio/hit_land.ogg");
    loadSfx(blockBuffer_,    blockPool_,    assetsDir + "audio/block_hit.ogg");
    loadSfx(clickBuffer_,    clickPool_,    assetsDir + "audio/ui_click.ogg");
    loadSfx(matchEndBuffer_, matchEndPool_, assetsDir + "audio/match_end.ogg");
}

void AudioManager::playMusic(MusicTrack track) {
    if (currentTrack_ == track) return;

    titleMusic_.stop();
    battleMusic_.stop();

    if (track == MusicTrack::Title && titleMusicLoaded_) {
        titleMusic_.play();
    } else if (track == MusicTrack::Battle && battleMusicLoaded_) {
        battleMusic_.play();
    }
    currentTrack_ = track;
}

void AudioManager::stopMusic() {
    titleMusic_.stop();
    battleMusic_.stop();
    currentTrack_ = MusicTrack::None;
}

void AudioManager::playAttackSwing()  { swingPool_.play(85.f); }
void AudioManager::playParrySuccess() { parryPool_.play(100.f); }
void AudioManager::playHitLand()      { hitPool_.play(90.f); }
void AudioManager::playBlockHit()     { blockPool_.play(80.f); }
void AudioManager::playUiClick()      { clickPool_.play(70.f); }
void AudioManager::playMatchEnd()     { matchEndPool_.play(100.f); }
