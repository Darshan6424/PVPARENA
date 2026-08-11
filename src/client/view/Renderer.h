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
#include <string>
#include <vector>

class Renderer {
public:
    explicit Renderer(const Assets& assets) : assets_(assets) {}

    void drawArena(sf::RenderWindow& window);

    // Backdrop for the menu screens. Dimmed and shaded so text stays readable.
    void drawTitleBackground(sf::RenderWindow& window);

    // stateTime is how long this player has been in its current state, which
    // the caller tracks because only it knows when the snapshot changed.
    // povId is the player to mark as "you", or NO_WINNER for local co-op.
    void drawPlayer(sf::RenderWindow& window, const net::PlayerSnapshot& snap,
                    int playerIndex, uint8_t povId, float stateTime);

    void drawHud(sf::RenderWindow& window, const net::StateUpdatePacket& state,
                 uint8_t povId);

    void spawnHitEffect(float x, float y);

    // Floating "-14" above a hit. Blocked hits get their own colour.
    void spawnDamageNumber(float x, float y, float damage, bool blocked);

    void updateAndDrawEffects(sf::RenderWindow& window, float dt);

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

    struct DamageNumber {
        sf::Vector2f pos;
        float age = 0.f;
        std::string text;
        sf::Color color;
    };

    void drawHitEffects(sf::RenderWindow& window, float dt);
    void drawDamageNumbers(sf::RenderWindow& window, float dt);

    void drawPlayerSprite(sf::RenderWindow& window, const net::PlayerSnapshot& snap,
                          int playerIndex, float stateTime);
    void drawPlayerShape(sf::RenderWindow& window, const net::PlayerSnapshot& snap,
                         int playerIndex, uint8_t povId);

    const Assets& assets_;
    std::vector<HitEffect> effects_;
    std::vector<DamageNumber> damageNumbers_;

    // Which walk-cycle row each player is using. A render frame that falls
    // between two network updates sees no movement at all, so the row is only
    // changed on movement big enough to be real, and held otherwise. Without
    // this the walk animation flickers toward the left/right fallback.
    std::array<sf::Vector2f, net::MAX_PLAYERS> lastPos_{};
    std::array<int, net::MAX_PLAYERS> lastWalkRow_{ -1, -1 };
    std::array<bool, net::MAX_PLAYERS> haveLastPos_{};
};
