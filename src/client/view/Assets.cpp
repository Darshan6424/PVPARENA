#include "client/view/Assets.h"
#include <cstdio>

namespace {

// The manifest. Order has to match the enums in Assets.h.
const char* const kTexturePaths[] = {
    "sprites/characterSheet.png",
    "sprites/skeletonSheet.png",
    "sprites/bloodParticle.png",
};

const char* const kSoundPaths[] = {
    "audio/attack_swing.ogg",
    "audio/parry_success.ogg",
    "audio/hit_land.ogg",
    "audio/block_hit.ogg",
    "audio/ui_click.ogg",
    "audio/match_end.ogg",
};

const char* const kMusicPaths[] = {
    "audio/title_music.ogg",
    "audio/battle_music.ogg",
};

const char* const kBundledFont = "fonts/DejaVuSans-Bold.ttf";

// Only used if the bundled font is missing, e.g. someone copied the exe
// without the assets folder.
const char* const kSystemFonts[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "C:\\Windows\\Fonts\\arialbd.ttf",
    "/System/Library/Fonts/Supplemental/Arial.ttf",
};

static_assert(sizeof(kTexturePaths) / sizeof(kTexturePaths[0]) ==
              static_cast<std::size_t>(Tex::Count), "texture manifest out of sync");
static_assert(sizeof(kSoundPaths) / sizeof(kSoundPaths[0]) ==
              static_cast<std::size_t>(Sfx::Count), "sound manifest out of sync");
static_assert(sizeof(kMusicPaths) / sizeof(kMusicPaths[0]) ==
              static_cast<std::size_t>(Track::Count), "music manifest out of sync");

} // namespace

bool Assets::loadAll() {
    missing_.clear();

    for (std::size_t i = 0; i < kTexCount; ++i) {
        texOk_[i] = textures_[i].loadFromFile(root_ + kTexturePaths[i]);
        if (texOk_[i]) {
            textures_[i].setSmooth(false);
        } else {
            missing_.push_back(kTexturePaths[i]);
        }
    }
    // The blood splash is scaled down a lot, so smoothing helps there.
    if (texOk_[index(Tex::Blood)]) textures_[index(Tex::Blood)].setSmooth(true);

    for (std::size_t i = 0; i < kSfxCount; ++i) {
        sfxOk_[i] = sounds_[i].loadFromFile(root_ + kSoundPaths[i]);
        if (!sfxOk_[i]) missing_.push_back(kSoundPaths[i]);
    }

    for (std::size_t i = 0; i < kTrackCount; ++i) {
        trackOk_[i] = music_[i].openFromFile(root_ + kMusicPaths[i]);
        if (trackOk_[i]) {
            music_[i].setLooping(true);
            music_[i].setVolume(55.f);
        } else {
            missing_.push_back(kMusicPaths[i]);
        }
    }

    if (!loadFont()) missing_.push_back(kBundledFont);

    if (!missing_.empty()) {
        std::fprintf(stderr, "Assets: %zu file(s) missing under %s\n",
                     missing_.size(), root_.c_str());
        for (const std::string& m : missing_) {
            std::fprintf(stderr, "  %s\n", m.c_str());
        }
    }
    return missing_.empty();
}

bool Assets::loadFont() {
    fontOk_ = font_.openFromFile(root_ + kBundledFont);
    if (fontOk_) return true;

    for (const char* path : kSystemFonts) {
        if (font_.openFromFile(path)) {
            fontOk_ = true;
            std::fprintf(stderr, "Assets: bundled font missing, using %s\n", path);
            return false;   // still report it: the shipped copy should be there
        }
    }
    std::fprintf(stderr, "Assets: no font available - text will not be drawn.\n");
    return false;
}
