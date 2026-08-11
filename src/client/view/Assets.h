#pragma once
// Every file the game loads from disk goes through here. One object owns them
// all, one place knows the paths, and anything that needs a texture or a sound
// borrows it by reference.
//
// Storage is fixed-size arrays indexed by the enums below. That keeps the
// addresses stable for the whole run, which matters because sf::Sprite holds a
// reference to its texture rather than a copy.

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <array>
#include <string>
#include <vector>

enum class Tex { Character, Skeleton, Blood, FloorTexture, FloorDecor, TitleBackground, Count };
enum class Sfx { AttackSwing, ParrySuccess, HitLand, BlockHit, UiClick, MatchEnd, Count };
enum class Track { Title, Battle, Count };

class Assets {
public:
    // root is the assets folder, with a trailing slash.
    explicit Assets(std::string root) : root_(std::move(root)) {}

    // Loads everything. Returns false if anything is missing, but the game is
    // still playable - callers check has() and fall back.
    bool loadAll();

    bool has(Tex id) const { return texOk_[index(id)]; }
    bool has(Sfx id) const { return sfxOk_[index(id)]; }
    bool has(Track id) const { return trackOk_[index(id)]; }
    bool hasFont() const { return fontOk_; }

    const sf::Texture& texture(Tex id) const { return textures_[index(id)]; }
    const sf::SoundBuffer& sound(Sfx id) const { return sounds_[index(id)]; }
    const sf::Font& font() const { return font_; }
    sf::Music& music(Track id) { return music_[index(id)]; }

    // Relative paths of anything that failed to load, for one clear report.
    const std::vector<std::string>& missing() const { return missing_; }

private:
    template <typename E>
    static std::size_t index(E id) { return static_cast<std::size_t>(id); }

    static constexpr std::size_t kTexCount   = static_cast<std::size_t>(Tex::Count);
    static constexpr std::size_t kSfxCount   = static_cast<std::size_t>(Sfx::Count);
    static constexpr std::size_t kTrackCount = static_cast<std::size_t>(Track::Count);

    bool loadFont();

    std::string root_;
    std::vector<std::string> missing_;

    std::array<sf::Texture, kTexCount> textures_;
    std::array<bool, kTexCount> texOk_{};

    std::array<sf::SoundBuffer, kSfxCount> sounds_;
    std::array<bool, kSfxCount> sfxOk_{};

    std::array<sf::Music, kTrackCount> music_;
    std::array<bool, kTrackCount> trackOk_{};

    sf::Font font_;
    bool fontOk_ = false;
};
