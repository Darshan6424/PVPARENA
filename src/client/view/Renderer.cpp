#include "client/view/Renderer.h"
#include <cmath>
#include <algorithm>

using namespace net;

namespace {

constexpr int TILE = 64;
constexpr float SPRITE_SCALE = 1.35f;
constexpr float WALK_FRAME_TIME = 0.09f;
constexpr float HIT_EFFECT_LIFETIME = 0.35f;
constexpr float DAMAGE_NUMBER_LIFETIME = 0.8f;
constexpr float DAMAGE_NUMBER_RISE = 34.f;

// Movement smaller than this between two frames is noise, not a direction
// change. Anything less and the walk row flickers between network updates.
constexpr float WALK_DIRECTION_EPS = 0.5f;

// First row of each 4-row group; +1 is left, +3 is right.
constexpr int ROW_SPELLCAST = 0;
constexpr int ROW_THRUST = 4;
constexpr int ROW_WALK = 8;
constexpr int ROW_SLASH = 12;
constexpr int ROW_HURT = 20;

constexpr int FRAMES_WALK = 9;
constexpr int FRAMES_SLASH = 6;
constexpr int FRAMES_HURT = 6;

int facingRow(int groupBase, float facing) {
    return groupBase + (facing >= 0.f ? 3 : 1);
}

int frameAt(float stateTime, float frameTime, int frameCount, bool loop) {
    int idx = static_cast<int>(stateTime / frameTime);
    if (loop) return ((idx % frameCount) + frameCount) % frameCount;
    return std::min(idx, frameCount - 1);
}

sf::IntRect tileRect(int row, int frame) {
    return sf::IntRect({ frame * TILE, row * TILE }, { TILE, TILE });
}

// What the sprite should look like this frame.
struct Pose {
    int row = ROW_WALK;
    int frame = 0;
    std::uint8_t alpha = 255;
};

// walkRow is the row the caller last saw this player actually walking along,
// or -1 if it has not seen real movement yet.
Pose poseFor(const PlayerSnapshot& snap, float stateTime, int walkRow) {
    Pose pose;
    switch (snap.state) {
    case ActionState::Idle:
        pose.row = facingRow(ROW_WALK, snap.facing);
        break;

    case ActionState::Moving:
        pose.row = (walkRow >= 0) ? walkRow : facingRow(ROW_WALK, snap.facing);
        pose.frame = frameAt(stateTime, WALK_FRAME_TIME, FRAMES_WALK, true);
        break;

    case ActionState::Attacking: {
        pose.row = facingRow(ROW_SLASH, snap.facing);
        float total = ATTACK_WINDUP + ATTACK_ACTIVE + ATTACK_RECOVERY;
        pose.frame = frameAt(stateTime, total / FRAMES_SLASH, FRAMES_SLASH, false);
        break;
    }

    case ActionState::Parrying:
        // No dedicated guard pose in the sheet, so the thrust wind-up stands in.
        pose.row = facingRow(ROW_THRUST, snap.facing);
        pose.frame = frameAt(stateTime, 0.05f, 4, false);
        break;

    case ActionState::Blocking:
        pose.row = facingRow(ROW_SPELLCAST, snap.facing);
        pose.frame = frameAt(stateTime, 0.08f, 4, false);
        break;

    case ActionState::Dodging:
        pose.row = facingRow(ROW_WALK, snap.facing);
        pose.frame = frameAt(stateTime, WALK_FRAME_TIME * 0.5f, FRAMES_WALK, true);
        pose.alpha = 165;
        break;

    case ActionState::Staggered:
        pose.row = ROW_HURT;   // only one direction of hurt art exists
        pose.frame = frameAt(stateTime, 0.6f / FRAMES_HURT, FRAMES_HURT, false);
        break;

    case ActionState::Dead:
        pose.row = ROW_HURT;
        pose.frame = FRAMES_HURT - 1;
        break;
    }
    return pose;
}

sf::Color stateColor(ActionState state, sf::Color base) {
    switch (state) {
    case ActionState::Attacking: return sf::Color(255, 210, 60);
    case ActionState::Parrying:  return sf::Color(80, 220, 255);
    case ActionState::Blocking:  return sf::Color(150, 150, 255);
    case ActionState::Dodging:   return sf::Color(200, 200, 200);
    case ActionState::Staggered: return sf::Color(255, 90, 90);
    case ActionState::Dead:      return sf::Color(60, 60, 60);
    default: return base;
    }
}

const char* stateLabel(ActionState state) {
    switch (state) {
    case ActionState::Attacking: return "ATTACK";
    case ActionState::Parrying:  return "PARRY";
    case ActionState::Blocking:  return "BLOCK";
    case ActionState::Dodging:   return "DODGE";
    case ActionState::Staggered: return "STAGGERED";
    case ActionState::Dead:      return "DOWN";
    default: return "";
    }
}

Tex textureFor(int playerIndex) {
    return playerIndex == 0 ? Tex::Character : Tex::Skeleton;
}

} // namespace

void Renderer::resetAnimationState() {
    lastPos_ = {};
    lastWalkRow_ = { -1, -1 };
    haveLastPos_ = {};
    effects_.clear();
    damageNumbers_.clear();
}

void Renderer::drawArena(sf::RenderWindow& window) {
    bool floorLoaded = assets_.has(Tex::FloorTexture) && assets_.has(Tex::FloorDecor);

    if (floorLoaded) {
        window.draw(sf::Sprite(assets_.texture(Tex::FloorTexture)));
        window.draw(sf::Sprite(assets_.texture(Tex::FloorDecor)));
    } else {
        sf::RectangleShape ground({ ARENA_WIDTH, ARENA_HEIGHT });
        ground.setPosition({ 0.f, 0.f });
        ground.setFillColor(sf::Color(40, 44, 52));
        window.draw(ground);
    }

    sf::RectangleShape midline({ 2.f, ARENA_HEIGHT });
    midline.setPosition({ ARENA_WIDTH / 2.f - 1.f, 0.f });
    midline.setFillColor(floorLoaded ? sf::Color(255, 255, 255, 40)
                                     : sf::Color(70, 74, 84));
    window.draw(midline);

    sf::RectangleShape border({ ARENA_WIDTH - 4.f, ARENA_HEIGHT - 4.f });
    border.setPosition({ 2.f, 2.f });
    border.setFillColor(sf::Color::Transparent);
    border.setOutlineColor(sf::Color(90, 95, 105));
    border.setOutlineThickness(3.f);
    window.draw(border);
}

void Renderer::drawTitleBackground(sf::RenderWindow& window) {
    if (!assets_.has(Tex::TitleBackground)) return;

    sf::Sprite bg(assets_.texture(Tex::TitleBackground));
    bg.setColor(sf::Color(160, 160, 160));   // dim it so the buttons stay readable
    window.draw(bg);

    // Even dimmed art competes with text, so a light shade goes over the top.
    sf::RectangleShape shade({ ARENA_WIDTH, ARENA_HEIGHT });
    shade.setPosition({ 0.f, 0.f });
    shade.setFillColor(sf::Color(10, 10, 15, 70));
    window.draw(shade);
}

void Renderer::drawPlayer(sf::RenderWindow& window, const PlayerSnapshot& snap,
                          int playerIndex, uint8_t povId, float stateTime) {
    if (!snap.connected) return;

    if (assets_.has(textureFor(playerIndex))) {
        drawPlayerSprite(window, snap, playerIndex, stateTime);
    } else {
        drawPlayerShape(window, snap, playerIndex, povId);
    }

    if (playerIndex == povId && assets_.hasFont()) {
        // Small dot under your own fighter so you can pick yourself out.
        sf::CircleShape marker(4.f);
        marker.setOrigin({ 4.f, 4.f });
        marker.setPosition({ snap.x, snap.y + (TILE * SPRITE_SCALE) / 2.f - 4.f });
        marker.setFillColor(sf::Color(255, 255, 255, 180));
        window.draw(marker);
    }

    const char* label = stateLabel(snap.state);
    if (label[0] != '\0') {
        drawCenteredText(window, label, snap.x, snap.y - PLAYER_RADIUS - 30.f, 14,
                         sf::Color::White);
    }
}

void Renderer::drawPlayerSprite(sf::RenderWindow& window, const PlayerSnapshot& snap,
                                int playerIndex, float stateTime) {
    sf::Vector2f pos{ snap.x, snap.y };

    // Pick the walk row from real movement only. A frame drawn between two
    // network updates sees a delta of zero, and reacting to that is what made
    // the walk cycle flicker.
    if (haveLastPos_[playerIndex]) {
        sf::Vector2f delta = pos - lastPos_[playerIndex];
        float absX = std::fabs(delta.x);
        float absY = std::fabs(delta.y);

        if (absY > absX && absY > WALK_DIRECTION_EPS) {
            lastWalkRow_[playerIndex] = ROW_WALK + (delta.y < 0.f ? 0 : 2);
        } else if (absX > WALK_DIRECTION_EPS) {
            lastWalkRow_[playerIndex] = ROW_WALK + (delta.x < 0.f ? 1 : 3);
        }
    }
    lastPos_[playerIndex] = pos;
    haveLastPos_[playerIndex] = true;

    Pose pose = poseFor(snap, stateTime, lastWalkRow_[playerIndex]);

    sf::Sprite sprite(assets_.texture(textureFor(playerIndex)));
    sprite.setTextureRect(tileRect(pose.row, pose.frame));
    sprite.setOrigin({ TILE / 2.f, TILE / 2.f });

    float bob = (snap.state == ActionState::Idle)
        ? std::sin(stateTime * 2.2f) * 1.6f
        : 0.f;
    sprite.setPosition({ snap.x, snap.y + bob });
    sprite.setScale({ SPRITE_SCALE, SPRITE_SCALE });
    sprite.setColor(sf::Color(255, 255, 255, pose.alpha));
    window.draw(sprite);
}

void Renderer::drawPlayerShape(sf::RenderWindow& window, const PlayerSnapshot& snap,
                               int playerIndex, uint8_t povId) {
    sf::Color base = (playerIndex == 0) ? sf::Color(90, 170, 255) : sf::Color(255, 120, 100);

    sf::CircleShape body(PLAYER_RADIUS);
    body.setOrigin({ PLAYER_RADIUS, PLAYER_RADIUS });
    body.setPosition({ snap.x, snap.y });
    body.setFillColor(stateColor(snap.state, base));
    body.setOutlineThickness(playerIndex == povId ? 3.f : 1.f);
    body.setOutlineColor(sf::Color::White);
    window.draw(body);

    sf::ConvexShape nose;
    nose.setPointCount(3);
    nose.setPoint(0, { snap.x + snap.facing * (PLAYER_RADIUS + 10.f), snap.y });
    nose.setPoint(1, { snap.x + snap.facing * PLAYER_RADIUS, snap.y - 6.f });
    nose.setPoint(2, { snap.x + snap.facing * PLAYER_RADIUS, snap.y + 6.f });
    nose.setFillColor(sf::Color::White);
    window.draw(nose);
}

void Renderer::spawnHitEffect(float x, float y) {
    if (!assets_.has(Tex::Blood)) return;
    effects_.push_back(HitEffect{ { x, y }, 0.f });
}

void Renderer::spawnDamageNumber(float x, float y, float damage, bool blocked) {
    if (!assets_.hasFont()) return;

    DamageNumber number;
    number.pos = { x, y };
    number.text = "-" + std::to_string(static_cast<int>(damage + 0.5f));
    number.color = blocked ? sf::Color(170, 190, 255) : sf::Color(255, 230, 90);
    damageNumbers_.push_back(number);
}

void Renderer::updateAndDrawEffects(sf::RenderWindow& window, float dt) {
    drawHitEffects(window, dt);
    drawDamageNumbers(window, dt);
}

void Renderer::drawHitEffects(sf::RenderWindow& window, float dt) {
    if (!assets_.has(Tex::Blood)) return;

    const sf::Texture& tex = assets_.texture(Tex::Blood);
    sf::Vector2u texSize = tex.getSize();

    for (auto it = effects_.begin(); it != effects_.end();) {
        it->age += dt;
        if (it->age >= HIT_EFFECT_LIFETIME) {
            it = effects_.erase(it);
            continue;
        }

        float t = it->age / HIT_EFFECT_LIFETIME;
        sf::Sprite sprite(tex);
        sprite.setOrigin({ texSize.x / 2.f, texSize.y / 2.f });
        sprite.setPosition(it->pos);
        sprite.setScale({ 0.15f + t * 0.15f, 0.15f + t * 0.15f });
        sprite.setColor(sf::Color(255, 255, 255,
                                  static_cast<std::uint8_t>(255.f * (1.f - t))));
        window.draw(sprite);
        ++it;
    }
}

void Renderer::drawDamageNumbers(sf::RenderWindow& window, float dt) {
    if (!assets_.hasFont()) return;

    for (auto it = damageNumbers_.begin(); it != damageNumbers_.end();) {
        it->age += dt;
        if (it->age >= DAMAGE_NUMBER_LIFETIME) {
            it = damageNumbers_.erase(it);
            continue;
        }

        float t = it->age / DAMAGE_NUMBER_LIFETIME;
        sf::Color faded = it->color;
        faded.a = static_cast<std::uint8_t>(255.f * (1.f - t * t)); // fades late

        drawCenteredText(window, it->text, it->pos.x, it->pos.y - t * DAMAGE_NUMBER_RISE,
                         18, faded);
        ++it;
    }
}

void Renderer::drawHud(sf::RenderWindow& window, const StateUpdatePacket& state,
                       uint8_t povId) {
    const float barWidth = 260.f;
    const float barHeight = 18.f;
    const float staminaHeight = 8.f;
    const float y = 16.f;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const PlayerSnapshot& snap = state.players[i];
        float x = (i == 0) ? 20.f : (ARENA_WIDTH - 20.f - barWidth);

        sf::RectangleShape hpBg({ barWidth, barHeight });
        hpBg.setPosition({ x, y });
        hpBg.setFillColor(sf::Color(50, 50, 50));
        window.draw(hpBg);

        sf::RectangleShape hpFill({ barWidth * (snap.health / MAX_HEALTH), barHeight });
        hpFill.setPosition({ x, y });
        hpFill.setFillColor(sf::Color(210, 60, 60));
        window.draw(hpFill);

        sf::RectangleShape stBg({ barWidth, staminaHeight });
        stBg.setPosition({ x, y + barHeight + 4.f });
        stBg.setFillColor(sf::Color(50, 50, 50));
        window.draw(stBg);

        sf::RectangleShape stFill({ barWidth * (snap.stamina / MAX_STAMINA), staminaHeight });
        stFill.setPosition({ x, y + barHeight + 4.f });
        stFill.setFillColor(sf::Color(60, 200, 120));
        window.draw(stFill);

        if (assets_.hasFont()) {
            std::string label;
            if (povId == NO_WINNER) {
                label = (i == 0) ? "P1" : "P2";
            } else {
                label = (i == povId) ? "YOU" : "OPPONENT";
            }
            sf::Text text(assets_.font(), label, 14);
            text.setPosition({ x, y - 18.f });
            text.setFillColor(sf::Color::White);
            window.draw(text);
        }
    }

    if (state.winnerId != NO_WINNER) {
        std::string msg;
        if (povId == NO_WINNER) {
            msg = (state.winnerId == 0) ? "PLAYER 1 WINS" : "PLAYER 2 WINS";
        } else {
            msg = (state.winnerId == povId) ? "YOU WIN" : "YOU LOSE";
        }
        drawCenteredText(window, msg, ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f - 20.f, 40,
                         sf::Color::Yellow);
        drawCenteredText(window, "R for a rematch, ESC for the title screen",
                         ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f + 30.f, 16, sf::Color::White);
    }
}

void Renderer::drawCenteredText(sf::RenderWindow& window, const std::string& text,
                                float x, float y, unsigned size, sf::Color color) {
    if (!assets_.hasFont()) return;

    sf::Text sfText(assets_.font(), text, size);
    sf::FloatRect bounds = sfText.getLocalBounds();
    sfText.setOrigin({ bounds.position.x + bounds.size.x / 2.f,
                       bounds.position.y + bounds.size.y / 2.f });
    sfText.setPosition({ x, y });
    sfText.setFillColor(color);
    window.draw(sfText);
}

void Renderer::drawButton(sf::RenderWindow& window, const sf::FloatRect& rect,
                          const std::string& label, bool hovered) {
    sf::RectangleShape box({ rect.size.x, rect.size.y });
    box.setPosition({ rect.position.x, rect.position.y });
    box.setFillColor(hovered ? sf::Color(90, 100, 120) : sf::Color(60, 65, 78));
    box.setOutlineColor(sf::Color::White);
    box.setOutlineThickness(2.f);
    window.draw(box);

    drawCenteredText(window, label,
                     rect.position.x + rect.size.x / 2.f,
                     rect.position.y + rect.size.y / 2.f,
                     20, sf::Color::White);
}
