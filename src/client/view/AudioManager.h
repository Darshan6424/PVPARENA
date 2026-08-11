#pragma once
// Music and one-shot sound effects. Purely reactive - it plays what the
// caller tells it to and decides nothing about the game.

#include "client/view/Assets.h"
#include <SFML/Audio.hpp>
#include <array>
#include <vector>

// A few sf::Sound instances sharing one buffer, handed out round-robin, so
// mashing attack doesn't cut the previous sound off.
class SoundPool {
public:
    void init(const sf::SoundBuffer& buffer, std::size_t size = 4);
    void play(float volume);

private:
    std::vector<sf::Sound> sounds_;
    std::size_t next_ = 0;
};

class AudioManager {
public:
    explicit AudioManager(Assets& assets) : assets_(assets) {}

    void init();

    void play(Sfx id);
    void playMusic(Track track);   // no-op if that track is already playing
    void stopMusic();

private:
    Assets& assets_;
    std::array<SoundPool, static_cast<std::size_t>(Sfx::Count)> pools_;
    int currentTrack_ = -1;
};
