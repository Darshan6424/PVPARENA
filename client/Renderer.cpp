#include "Renderer.h"
#include <array>
#include <cstdio>
#include <cmath>
#include <algorithm>

using namespace net;

namespace {
    constexpr int TILE = 64;

    // Row indices into the Universal LPC Spritesheet layout (see header
    // comment). Each constant is the "up" row of its 4-row group; left
    // is +1, down is +2, right is +3.
    constexpr int ROW_SPELLCAST = 0;
    constexpr int ROW_THRUST    = 4;
    constexpr int ROW_WALK      = 8;
    constexpr int ROW_SLASH     = 12;
    constexpr int ROW_HURT      = 20;

    constexpr int FRAMES_SPELLCAST = 7;
    constexpr int FRAMES_THRUST    = 8;
    constexpr int FRAMES_WALK      = 9;
    constexpr int FRAMES_SLASH     = 6;
    constexpr int FRAMES_HURT      = 6;

    constexpr float WALK_FRAME_TIME = 0.09f; // ~11 fps walk cycle
    constexpr float SPRITE_SCALE = 1.35f;

    int facingRow(int groupBase, float facing) {
        return facing >= 0.f ? groupBase + 3 : groupBase + 1; // right : left
    }

    int frameIndexForTime(float stateTime, float frameTime, int frameCount, bool loop) {
        int idx = static_cast<int>(stateTime / frameTime);
        if (loop) {
            return ((idx % frameCount) + frameCount) % frameCount;
        }
        return std::min(idx, frameCount - 1);
    }
}

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

void Renderer::loadTextures(const std::string& assetsDir) {
    bool charOk = characterTexture_.loadFromFile(assetsDir + "sprites/characterSheet.png");
    bool skelOk = skeletonTexture_.loadFromFile(assetsDir + "sprites/skeletonSheet.png");
    spritesLoaded_ = charOk && skelOk;
    if (!spritesLoaded_) {
        std::fprintf(stderr, "Renderer: character sprite sheets not found under %s - "
                              "falling back to flat shapes.\n", assetsDir.c_str());
    } else {
        characterTexture_.setSmooth(false);
        skeletonTexture_.setSmooth(false);
    }

    bloodLoaded_ = bloodTexture_.loadFromFile(assetsDir + "sprites/bloodParticle.png");
    if (bloodLoaded_) {
        bloodTexture_.setSmooth(true);
    }
}

sf::IntRect Renderer::frameRect(int animRow, int frameIndex) const {
    return sf::IntRect({ frameIndex * TILE, animRow * TILE }, { TILE, TILE });
}

void Renderer::drawArena(sf::RenderWindow& window) {
    sf::RectangleShape ground(sf::Vector2f(ARENA_WIDTH, ARENA_HEIGHT));
    ground.setPosition({ 0.f, 0.f });
    ground.setFillColor(sf::Color(40, 44, 52));
    window.draw(ground);

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
                           uint8_t playerIndex, uint8_t localPlayerId, float stateTime) {
    if (!snap.connected) return;

    if (spritesLoaded_) {
        sf::Texture& tex = (playerIndex == 0) ? characterTexture_ : skeletonTexture_;

        int row = ROW_WALK;
        int frameCount = FRAMES_WALK;
        int frameIdx = 0;
        bool loopAnim = true;
        float alpha = 255.f;

        switch (snap.state) {
            case ActionState::Idle:
                row = facingRow(ROW_WALK, snap.facing);
                frameIdx = 0;
                break;
            case ActionState::Moving:
                row = facingRow(ROW_WALK, snap.facing);
                frameCount = FRAMES_WALK;
                frameIdx = frameIndexForTime(stateTime, WALK_FRAME_TIME, frameCount, true);
                break;
            case ActionState::Attacking: {
                row = facingRow(ROW_SLASH, snap.facing);
                frameCount = FRAMES_SLASH;
                float totalDuration = ATTACK_WINDUP + ATTACK_ACTIVE + ATTACK_RECOVERY;
                float frameTime = totalDuration / static_cast<float>(frameCount);
                frameIdx = frameIndexForTime(stateTime, frameTime, frameCount, false);
                break;
            }
            case ActionState::Parrying: {
                row = facingRow(ROW_THRUST, snap.facing);
                frameCount = FRAMES_THRUST;
                frameIdx = frameIndexForTime(stateTime, 0.05f, 4, false); // quick rise, hold guard
                break;
            }
            case ActionState::Blocking: {
                row = facingRow(ROW_SPELLCAST, snap.facing);
                frameCount = FRAMES_SPELLCAST;
                frameIdx = frameIndexForTime(stateTime, 0.08f, 4, false); // quick raise, hold
                break;
            }
            case ActionState::Dodging: {
                row = facingRow(ROW_WALK, snap.facing);
                frameCount = FRAMES_WALK;
                frameIdx = frameIndexForTime(stateTime, WALK_FRAME_TIME * 0.5f, frameCount, true);
                alpha = 165.f; // ghostly look while invulnerable
                break;
            }
            case ActionState::Staggered: {
                row = ROW_HURT; // single canonical direction, no left/right art
                frameCount = FRAMES_HURT;
                float frameTime = 0.6f / static_cast<float>(frameCount);
                frameIdx = frameIndexForTime(stateTime, frameTime, frameCount, false);
                break;
            }
            case ActionState::Dead: {
                row = ROW_HURT;
                frameIdx = FRAMES_HURT - 1; // held on the final "down" frame
                break;
            }
        }

        sf::Sprite sprite(tex);
        sprite.setTextureRect(frameRect(row, frameIdx));
        sprite.setOrigin({ TILE / 2.f, TILE / 2.f });
        sprite.setPosition({ snap.x, snap.y });
        sprite.setScale({ SPRITE_SCALE, SPRITE_SCALE });
        sprite.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
        window.draw(sprite);

        // Thin ring under the local player's own character so it's easy
        // to tell yourself apart from the opponent at a glance.
        if (playerIndex == localPlayerId) {
            sf::CircleShape marker(4.f);
            marker.setOrigin({ 4.f, 4.f });
            marker.setPosition({ snap.x, snap.y + (TILE * SPRITE_SCALE) / 2.f - 4.f });
            marker.setFillColor(sf::Color(255, 255, 255, 180));
            window.draw(marker);
        }
    } else {
        // Fallback if the sprite sheets weren't found next to the exe.
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

        sf::ConvexShape facingMark;
        facingMark.setPointCount(3);
        float tipX = snap.x + snap.facing * (PLAYER_RADIUS + 10.f);
        facingMark.setPoint(0, { tipX, snap.y });
        facingMark.setPoint(1, { snap.x + snap.facing * PLAYER_RADIUS, snap.y - 6.f });
        facingMark.setPoint(2, { snap.x + snap.facing * PLAYER_RADIUS, snap.y + 6.f });
        facingMark.setFillColor(sf::Color::White);
        window.draw(facingMark);
    }

    const char* label = labelForState(snap.state);
    if (label[0] != '\0' && fontLoaded_) {
        drawCenteredText(window, label, snap.x, snap.y - PLAYER_RADIUS - 30.f, 14,
                          sf::Color::White);
    }
}

void Renderer::spawnHitEffect(sf::Vector2f pos) {
    if (!bloodLoaded_) return;
    HitEffect fx;
    fx.pos = pos;
    fx.age = 0.f;
    hitEffects_.push_back(fx);
}

void Renderer::updateAndDrawHitEffects(sf::RenderWindow& window, float dt) {
    if (!bloodLoaded_) return;

    for (auto it = hitEffects_.begin(); it != hitEffects_.end();) {
        it->age += dt;
        if (it->age >= HitEffect::LIFETIME) {
            it = hitEffects_.erase(it);
            continue;
        }

        float t = it->age / HitEffect::LIFETIME;
        float scale = 0.15f + t * 0.15f;              // grows slightly
        float alpha = 255.f * (1.f - t);                // fades out

        sf::Sprite sprite(bloodTexture_);
        sf::Vector2u texSize = bloodTexture_.getSize();
        sprite.setOrigin({ texSize.x / 2.f, texSize.y / 2.f });
        sprite.setPosition(it->pos);
        sprite.setScale({ scale, scale });
        sprite.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
        window.draw(sprite);

        ++it;
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

        sf::RectangleShape hpBg(sf::Vector2f(barWidth, barHeight));
        hpBg.setPosition({ x, y });
        hpBg.setFillColor(sf::Color(50, 50, 50));
        window.draw(hpBg);

        float hpRatio = snap.health / MAX_HEALTH;
        sf::RectangleShape hpFill(sf::Vector2f(barWidth * hpRatio, barHeight));
        hpFill.setPosition({ x, y });
        hpFill.setFillColor(sf::Color(210, 60, 60));
        window.draw(hpFill);

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
