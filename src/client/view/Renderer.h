#pragma once
// Drawing only. Everything here reads from a snapshot the server sent; it
// never works out positions, health or outcomes itself.
//
// Characters use the Universal LPC Spritesheet layout: 64x64 tiles, 13 columns
// by 21 rows, grouped in fours (up, left, down, right):
//   rows  0-3  spellcast, 7 frames      rows 12-15  slash, 6 frames
//   rows  4-7  thrust, 8 frames         rows 16-19  shoot, 13 frames
//   rows  8-11 walk, 9 frames           row  20     hurt, 6 frames

#include "client/view/Assets.h"
#include "common/Protocol.h"
#include <SFML/Graphics.hpp>
#include <array>
#include <vector>

class Renderer {
public:
    explicit Renderer(const Assets& assets) : assets_(assets) {}

    void drawArena(sf::RenderWindow& window);

    // stateTime is how long this player has been in its current state, which
    // the caller tracks because only it knows when the snapshot changed.
    // povId is the player to mark as "you", or NO_WINNER for local co-op.
    void drawPlayer(sf::RenderWindow& window, const net::PlayerSnapshot& snap,
                    int playerIndex, uint8_t povId, float stateTime);

    void drawHud(sf::RenderWindow& window, const net::StateUpdatePacket& state,
                 uint8_t povId);

    void spawnHitEffect(float x, float y);
    void updateAndDrawHitEffects(sf::RenderWindow& window, float dt);
    void clearHitEffects() { effects_.clear(); }

    void drawCenteredText(sf::RenderWindow& window, const std::string& text,
                          float x, float y, unsigned size, sf::Color color);
    void drawButton(sf::RenderWindow& window, const sf::FloatRect& rect,
                    const std::string& label, bool hovered);

    void resetAnimationState();

private:
    struct HitEffect {
        sf::Vector2f pos;
        float age = 0.f;
    };

    void drawPlayerSprite(sf::RenderWindow& window, const net::PlayerSnapshot& snap,
                          int playerIndex, float stateTime);
    void drawPlayerShape(sf::RenderWindow& window, const net::PlayerSnapshot& snap,
                         int playerIndex, uint8_t povId);

    const Assets& assets_;
    std::vector<HitEffect> effects_;

    // Last direction each player was seen moving, used to pick a walk row.
    // Kept between frames so a repeated snapshot doesn't make it flicker.
    std::array<sf::Vector2f, net::MAX_PLAYERS> lastPos_{};
    std::array<sf::Vector2f, net::MAX_PLAYERS> lastDir_{};
    std::array<bool, net::MAX_PLAYERS> haveLastPos_{};
};
