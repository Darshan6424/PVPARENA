#include "Renderer.h"
#include <array>
#include <cstdio>

using namespace net;

Renderer::Renderer() {
    loadFont();
}

void Renderer::loadFont() {
    static const std::array<const char*, 6> candidates = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\arialbd.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    };

    for (const char* path : candidates) {
        if (font_.openFromFile(path)) {
            fontLoaded_ = true;
            return;
        }
    }
    std::fprintf(stderr, "Renderer: no system font found - UI text will be skipped.\n");
}

void Renderer::drawArena(sf::RenderWindow& window) {
    sf::RectangleShape ground(sf::Vector2f(ARENA_WIDTH, ARENA_HEIGHT));
    ground.setPosition({ 0.f, 0.f });
    ground.setFillColor(sf::Color(40, 44, 52));
    window.draw(ground);

    // Center line so players have a visual sense of the midpoint.
    sf::RectangleShape midline(sf::Vector2f(2.f, ARENA_HEIGHT));
    midline.setPosition({ ARENA_WIDTH / 2.f - 1.f, 0.f });
    midline.setFillColor(sf::Color(70, 74, 84));
    window.draw(midline);

    sf::RectangleShape border(sf::Vector2f(ARENA_WIDTH - 4.f, ARENA_HEIGHT - 4.f));
    border.setPosition({ 2.f, 2.f });
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(sf::Color(90, 95, 105));
    border.setOutlineThickness(3.f);
    window.draw(border);
}

namespace {
    sf::Color colorForState(ActionState state, sf::Color baseColor) {
        switch (state) {
            case ActionState::Attacking: return sf::Color(255, 210, 60);
            case ActionState::Parrying:  return sf::Color(80, 220, 255);
            case ActionState::Blocking:  return sf::Color(150, 150, 255);
            case ActionState::Dodging:   return sf::Color(200, 200, 200);
            case ActionState::Staggered: return sf::Color(255, 90, 90);
            case ActionState::Dead:      return sf::Color(60, 60, 60);
            default: return baseColor;
        }
    }

    const char* labelForState(ActionState state) {
        switch (state) {
            case ActionState::Idle:      return "";
            case ActionState::Moving:    return "";
            case ActionState::Attacking: return "ATTACK";
            case ActionState::Parrying:  return "PARRY";
            case ActionState::Blocking:  return "BLOCK";
            case ActionState::Dodging:   return "DODGE";
            case ActionState::Staggered: return "STAGGERED";
            case ActionState::Dead:      return "DOWN";
        }
        return "";
    }
}

void Renderer::drawPlayer(sf::RenderWindow& window, const PlayerSnapshot& snap,
                           uint8_t playerIndex, uint8_t localPlayerId) {
    if (!snap.connected) return;

    sf::Color baseColor = (playerIndex == 0) ? sf::Color(90, 170, 255)
                                              : sf::Color(255, 120, 100);
    sf::Color color = colorForState(snap.state, baseColor);

    sf::CircleShape body(PLAYER_RADIUS);
    body.setOrigin({ PLAYER_RADIUS, PLAYER_RADIUS });
    body.setPosition({ snap.x, snap.y });
    body.setFillColor(color);
    body.setOutlineThickness(playerIndex == localPlayerId ? 3.f : 1.f);
    body.setOutlineColor(sf::Color::White);
    window.draw(body);

    // Small facing indicator so it's readable which way each fighter is oriented.
    sf::ConvexShape facingMark;
    facingMark.setPointCount(3);
    float tipX = snap.x + snap.facing * (PLAYER_RADIUS + 10.f);
    facingMark.setPoint(0, { tipX, snap.y });
    facingMark.setPoint(1, { snap.x + snap.facing * PLAYER_RADIUS, snap.y - 6.f });
    facingMark.setPoint(2, { snap.x + snap.facing * PLAYER_RADIUS, snap.y + 6.f });
    facingMark.setFillColor(sf::Color::White);
    window.draw(facingMark);

    const char* label = labelForState(snap.state);
    if (label[0] != '\0' && fontLoaded_) {
        drawCenteredText(window, label, snap.x, snap.y - PLAYER_RADIUS - 18.f, 14,
                          sf::Color::White);
    }
}

void Renderer::drawHud(sf::RenderWindow& window, const StateUpdatePacket& state,
                        uint8_t localPlayerId) {
    const float barWidth = 260.f;
    const float barHeight = 18.f;
    const float staminaHeight = 8.f;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const PlayerSnapshot& snap = state.players[i];
        bool onLeft = (i == 0);
        float x = onLeft ? 20.f : (ARENA_WIDTH - 20.f - barWidth);
        float y = 16.f;

        // Health bar background + fill.
        sf::RectangleShape hpBg(sf::Vector2f(barWidth, barHeight));
        hpBg.setPosition({ x, y });
        hpBg.setFillColor(sf::Color(50, 50, 50));
        window.draw(hpBg);

        float hpRatio = snap.health / MAX_HEALTH;
        sf::RectangleShape hpFill(sf::Vector2f(barWidth * hpRatio, barHeight));
        hpFill.setPosition({ x, y });
        hpFill.setFillColor(sf::Color(210, 60, 60));
        window.draw(hpFill);

        // Stamina bar just underneath.
        sf::RectangleShape stBg(sf::Vector2f(barWidth, staminaHeight));
        stBg.setPosition({ x, y + barHeight + 4.f });
        stBg.setFillColor(sf::Color(50, 50, 50));
        window.draw(stBg);

        float stRatio = snap.stamina / MAX_STAMINA;
        sf::RectangleShape stFill(sf::Vector2f(barWidth * stRatio, staminaHeight));
        stFill.setPosition({ x, y + barHeight + 4.f });
        stFill.setFillColor(sf::Color(60, 200, 120));
        window.draw(stFill);

        if (fontLoaded_) {
            std::string label = (i == localPlayerId) ? "YOU" : "OPPONENT";
            sf::Text text(font_, label, 14);
            text.setPosition({ x, y - 18.f });
            text.setFillColor(sf::Color::White);
            window.draw(text);
        }
    }

    if (state.winnerId != 255 && fontLoaded_) {
        std::string msg = (state.winnerId == localPlayerId) ? "YOU WIN" : "YOU LOSE";
        drawCenteredText(window, msg, ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f - 20.f, 40,
                          sf::Color::Yellow);
        drawCenteredText(window, "Press ESC to return to title", ARENA_WIDTH / 2.f,
                          ARENA_HEIGHT / 2.f + 30.f, 16, sf::Color::White);
    }
}

void Renderer::drawCenteredText(sf::RenderWindow& window, const std::string& text,
                                 float x, float y, unsigned int size, sf::Color color) {
    if (!fontLoaded_) return;
    sf::Text sfText(font_, text, size);
    sf::FloatRect bounds = sfText.getLocalBounds();
    sfText.setOrigin({ bounds.position.x + bounds.size.x / 2.f,
                        bounds.position.y + bounds.size.y / 2.f });
    sfText.setPosition({ x, y });
    sfText.setFillColor(color);
    window.draw(sfText);
}
