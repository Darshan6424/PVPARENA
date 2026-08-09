#pragma once
// Renderer.h
// Pure presentation. Every draw call here reads from a StateUpdatePacket
// the server sent - it never decides positions, health, or outcomes
// itself. Animation frame selection is cosmetic-only: it's driven by
// how long (client-side, wall-clock) a player has been in their current
// server-reported ActionState, never by anything the client decides on
// its own.
//
// Character sprites use the "Universal LPC Spritesheet" layout: 64x64
// tiles, 13 columns x 21 rows.
//   Rows  0- 3: Spellcast   (up, left, down, right) - 7 frames
//   Rows  4- 7: Thrust      (up, left, down, right) - 8 frames
//   Rows  8-11: Walk        (up, left, down, right) - 9 frames
//   Rows 12-15: Slash       (up, left, down, right) - 6 frames
//   Rows 16-19: Shoot       (up, left, down, right) - 13 frames
//   Row  20:    Hurt        (single direction)      - 6 frames
// We only ever use the "left" and "right" rows of each group since this
// game only distinguishes left/right facing.

#include "../common/Protocol.h"
#include <SFML/Graphics.hpp>
#include <array>
#include <vector>

// One-shot cosmetic effect (e.g. a blood splash where a hit landed).
// Purely decorative - spawned client-side when it *observes* health
// drop between two StateUpdate packets, never something the server is
// told about or asked to authorize.
struct HitEffect {
    sf::Vector2f pos;
    float age = 0.f;
    static constexpr float LIFETIME = 0.35f;
};

class Renderer {
public:
    Renderer();

    // Tries a handful of common system font paths. If none are found,
    // text simply won't be drawn - shapes and bars still work fine.
    void loadFont();

    // Loads character sheets / particle texture from assetsDir (with
    // trailing slash). Safe to call even if files are missing - falls
    // back to flat-colored shapes so the game still runs.
    void loadTextures(const std::string& assetsDir);

    void drawArena(sf::RenderWindow& window);

    // stateTime = seconds since this player's ActionState last changed
    // (tracked by the caller, since only the caller knows when the
    // server's snapshot actually changed).
    void drawPlayer(sf::RenderWindow& window, const net::PlayerSnapshot& snap,
                     uint8_t playerIndex, uint8_t localPlayerId, float stateTime);

    void drawHud(sf::RenderWindow& window, const net::StateUpdatePacket& state,
                 uint8_t localPlayerId);

    void updateAndDrawHitEffects(sf::RenderWindow& window, float dt);
    void spawnHitEffect(sf::Vector2f pos);

    void drawCenteredText(sf::RenderWindow& window, const std::string& text,
                           float x, float y, unsigned int size, sf::Color color);

    bool fontLoaded() const { return fontLoaded_; }
    const sf::Font& font() const { return font_; }
    bool spritesLoaded() const { return spritesLoaded_; }

private:
    sf::IntRect frameRect(int animRow, int frameIndex) const;

    sf::Font font_;
    bool fontLoaded_ = false;

    sf::Texture characterTexture_;
    sf::Texture skeletonTexture_;
    sf::Texture bloodTexture_;
    bool spritesLoaded_ = false;
    bool bloodLoaded_ = false;

    std::vector<HitEffect> hitEffects_;
};
