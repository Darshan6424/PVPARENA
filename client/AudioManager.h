#pragma once
// AudioManager.h
// Background music + one-shot sound effects. Purely presentational,
// same as Renderer - it reacts to what the server told the client
// already happened (a hit landed, someone parried, health dropped);
// it never decides anything.

#include <SFML/Audio.hpp>
#include <string>
#include <vector>
#include <cstddef>

enum class MusicTrack {
    None,
    Title,
    Battle,
};

// A tiny round-robin pool of sf::Sound instances sharing one buffer, so
// rapid repeated triggers (e.g. mashing attack) don't cut each other
// off - each call to play() just claims the next slot in the pool.
class SoundPool {
public:
    void init(const sf::SoundBuffer& buffer, std::size_t poolSize = 4);
    void play(float volume = 100.f);

private:
    std::vector<sf::Sound> sounds_;
    std::size_t nextIndex_ = 0;
};

class AudioManager {
public:
    // Loads everything from assetsDir (with trailing slash). Safe to
    // call even if some/all files are missing - the game just plays
    // silently for whichever piece didn't load.
    void loadAll(const std::string& assetsDir);

    void playMusic(MusicTrack track);
    void stopMusic();

    void playAttackSwing();
    void playParrySuccess();
    void playHitLand();
    void playBlockHit();
    void playUiClick();
    void playMatchEnd();

private:
    sf::Music titleMusic_;
    sf::Music battleMusic_;
    bool titleMusicLoaded_ = false;
    bool battleMusicLoaded_ = false;
    MusicTrack currentTrack_ = MusicTrack::None;

    sf::SoundBuffer swingBuffer_;
    sf::SoundBuffer parryBuffer_;
    sf::SoundBuffer hitBuffer_;
    sf::SoundBuffer blockBuffer_;
    sf::SoundBuffer clickBuffer_;
    sf::SoundBuffer matchEndBuffer_;

    SoundPool swingPool_;
    SoundPool parryPool_;
    SoundPool hitPool_;
    SoundPool blockPool_;
    SoundPool clickPool_;
    SoundPool matchEndPool_;
};
