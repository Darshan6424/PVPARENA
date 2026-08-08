#include "GameServer.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>

using namespace net;

// -----------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------

bool GameServer::start(uint16_t port) {
    if (!sock_.bind(port)) {
        std::fprintf(stderr, "GameServer: failed to bind port %u\n", port);
        return false;
    }

    players_[0] = ServerPlayer{};
    players_[1] = ServerPlayer{};
    players_[0].pos = { 120.f, ARENA_HEIGHT / 2.f };
    players_[1].pos = { ARENA_WIDTH - 120.f, ARENA_HEIGHT / 2.f };

    std::printf("GameServer: listening on UDP port %u\n", port);
    return true;
}

void GameServer::stop() {
    sock_.close();
}

// -----------------------------------------------------------------------
// Main loop - fixed tickrate, sleeps off any spare time each iteration.
// -----------------------------------------------------------------------

void GameServer::run(std::atomic<bool>& keepRunning) {
    using clock = std::chrono::steady_clock;
    auto lastTick = clock::now();
    const auto tickDuration = std::chrono::duration<double>(TICK_DT);

    while (keepRunning.load()) {
        handleIncomingPackets();

        auto now = clock::now();
        std::chrono::duration<double> elapsed = now - lastTick;
        if (elapsed.count() >= TICK_DT) {
            tick(static_cast<float>(elapsed.count()));
            lastTick = now;
            broadcastState();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// -----------------------------------------------------------------------
// Networking
// -----------------------------------------------------------------------

void GameServer::handleIncomingPackets() {
    char buffer[512];
    Endpoint from;

    while (true) {
        int received = sock_.receive(buffer, sizeof(buffer), from);
        if (received <= 0) break; // 0 = nothing waiting, -1 = error

        if (received < static_cast<int>(sizeof(PacketHeader))) continue;

        PacketHeader header;
        std::memcpy(&header, buffer, sizeof(PacketHeader));
        if (header.magic != PROTOCOL_MAGIC) continue;

        switch (header.type) {
            case PacketType::ConnectRequest:
                handleConnectRequest(from);
                break;

            case PacketType::InputState: {
                if (received != static_cast<int>(sizeof(InputStatePacket))) break;
                InputStatePacket pkt;
                std::memcpy(&pkt, buffer, sizeof(pkt));
                handleInputPacket(pkt);
                break;
            }

            case PacketType::Disconnect: {
                if (received != static_cast<int>(sizeof(DisconnectPacket))) break;
                DisconnectPacket pkt;
                std::memcpy(&pkt, buffer, sizeof(pkt));
                if (pkt.playerId < MAX_PLAYERS) {
                    players_[pkt.playerId].connected = false;
                }
                break;
            }

            default:
                break;
        }
    }
}

void GameServer::handleConnectRequest(const Endpoint& from) {
    // Already-connected client retrying its request - just re-ack it.
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].connected && players_[i].endpoint == from) {
            ConnectAcceptPacket accept;
            accept.assignedPlayerId = static_cast<uint8_t>(i);
            sock_.send(&accept, sizeof(accept), from);
            return;
        }
    }

    // Find a free slot.
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!players_[i].connected) {
            players_[i].connected = true;
            players_[i].endpoint = from;
            players_[i].health = MAX_HEALTH;
            players_[i].stamina = MAX_STAMINA;
            players_[i].state = ActionState::Idle;
            players_[i].pos = (i == 0)
                ? Vec2{ 120.f, ARENA_HEIGHT / 2.f }
                : Vec2{ ARENA_WIDTH - 120.f, ARENA_HEIGHT / 2.f };

            ConnectAcceptPacket accept;
            accept.assignedPlayerId = static_cast<uint8_t>(i);
            sock_.send(&accept, sizeof(accept), from);

            std::printf("GameServer: player %d connected from %s:%u\n",
                        i, from.ip.c_str(), from.port);
            return;
        }
    }

    ConnectRejectPacket reject;
    std::snprintf(reject.reason, sizeof(reject.reason), "Server full");
    sock_.send(&reject, sizeof(reject), from);
}

void GameServer::handleInputPacket(const InputStatePacket& pkt) {
    if (pkt.playerId >= MAX_PLAYERS) return;
    ServerPlayer& p = players_[pkt.playerId];
    if (!p.connected || p.endpoint.ip.empty()) return;

    // Only accept newer inputs (UDP can reorder packets).
    if (pkt.sequence != 0 && pkt.sequence < p.lastInput.sequence) return;

    p.lastInput = pkt;
    p.hadInputThisTick = true;
}

// -----------------------------------------------------------------------
// Simulation
// -----------------------------------------------------------------------

void GameServer::tick(float dt) {
    ++tickCounter_;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].connected) {
            updatePlayer(i, dt);
        }
    }

    resolvePlayerCollision();

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        players_[i].hadInputThisTick = false;
    }
}

void GameServer::resolvePlayerCollision() {
    if (!players_[0].connected || !players_[1].connected) return;
    if (players_[0].state == ActionState::Dead || players_[1].state == ActionState::Dead) return;

    ServerPlayer& a = players_[0];
    ServerPlayer& b = players_[1];

    Vec2 delta = b.pos - a.pos;
    float dist = delta.length();
    const float minSeparation = PLAYER_RADIUS * 2.f;

    if (dist < minSeparation && dist > 1e-4f) {
        Vec2 pushDir = delta.normalized();
        float overlap = minSeparation - dist;
        a.pos = a.pos - pushDir * (overlap * 0.5f);
        b.pos = b.pos + pushDir * (overlap * 0.5f);
    } else if (dist <= 1e-4f) {
        // Degenerate case: exactly overlapping. Nudge apart arbitrarily.
        a.pos = a.pos - Vec2{ minSeparation * 0.5f, 0.f };
        b.pos = b.pos + Vec2{ minSeparation * 0.5f, 0.f };
    }

    a.pos.x = clampf(a.pos.x, PLAYER_RADIUS, ARENA_WIDTH - PLAYER_RADIUS);
    a.pos.y = clampf(a.pos.y, PLAYER_RADIUS, ARENA_HEIGHT - PLAYER_RADIUS);
    b.pos.x = clampf(b.pos.x, PLAYER_RADIUS, ARENA_WIDTH - PLAYER_RADIUS);
    b.pos.y = clampf(b.pos.y, PLAYER_RADIUS, ARENA_HEIGHT - PLAYER_RADIUS);
}

void GameServer::updatePlayer(int id, float dt) {
    ServerPlayer& p = players_[id];
    int otherId = 1 - id;
    ServerPlayer& other = players_[otherId];

    if (p.state == ActionState::Dead) return;

    // Face the opponent automatically - this is a simple duel, not a
    // platformer, so there's no separate "turn around" input.
    if (other.pos.x != p.pos.x) {
        p.facing = (other.pos.x > p.pos.x) ? 1.f : -1.f;
    }

    // ---- Advance whatever timed action is currently in progress -------
    bool locked = (p.state == ActionState::Attacking ||
                   p.state == ActionState::Parrying  ||
                   p.state == ActionState::Dodging   ||
                   p.state == ActionState::Staggered);

    if (locked) {
        p.actionTimer -= dt;

        if (p.state == ActionState::Attacking) {
            float elapsed = ATTACK_WINDUP + ATTACK_ACTIVE + ATTACK_RECOVERY - p.actionTimer;
            bool inActiveWindow = elapsed >= ATTACK_WINDUP &&
                                   elapsed < (ATTACK_WINDUP + ATTACK_ACTIVE);
            if (inActiveWindow && !p.attackHasHit) {
                resolveAttackHit(id);
            }
        } else if (p.state == ActionState::Dodging) {
            // Move for the whole dodge; only the first slice is invulnerable.
            Vec2 dodgeDir{ p.lastInput.moveX, p.lastInput.moveY };
            if (dodgeDir.length() < 0.01f) dodgeDir = { p.facing, 0.f };
            dodgeDir = dodgeDir.normalized();

            p.pos = p.pos + dodgeDir * (DODGE_SPEED * dt);
            p.pos.x = clampf(p.pos.x, PLAYER_RADIUS, ARENA_WIDTH - PLAYER_RADIUS);
            p.pos.y = clampf(p.pos.y, PLAYER_RADIUS, ARENA_HEIGHT - PLAYER_RADIUS);

            float elapsed = DODGE_DURATION - p.actionTimer;
            p.dodgeInvulnerable = elapsed < DODGE_IFRAME_TIME;
        }

        if (p.actionTimer <= 0.f) {
            p.state = ActionState::Idle;
            p.dodgeInvulnerable = false;
        }
        return; // no new actions while locked
    }

    // ---- Not locked: read this tick's input and act on it -------------
    // attack/parry/dodge are edge-triggered: consume them here so a
    // packet that arrives once but doesn't get overwritten before the
    // next tick (e.g. client hiccup, or a currently-locked player only
    // just returning to Idle) can never fire the same action twice.
    InputStatePacket in = p.lastInput;
    p.lastInput.attack = 0;
    p.lastInput.parry = 0;
    p.lastInput.dodge = 0;

    bool spentStaminaThisTick = false;

    if (in.attack && p.stamina >= ATTACK_STAMINA_COST) {
        p.state = ActionState::Attacking;
        p.actionTimer = ATTACK_WINDUP + ATTACK_ACTIVE + ATTACK_RECOVERY;
        p.attackHasHit = false;
        p.stamina -= ATTACK_STAMINA_COST;
        spentStaminaThisTick = true;
    } else if (in.parry && p.stamina >= PARRY_STAMINA_COST) {
        p.state = ActionState::Parrying;
        p.actionTimer = PARRY_WINDUP + PARRY_WINDOW + PARRY_RECOVERY;
        p.stamina -= PARRY_STAMINA_COST;
        spentStaminaThisTick = true;
    } else if (in.dodge && p.stamina >= DODGE_STAMINA_COST) {
        p.state = ActionState::Dodging;
        p.actionTimer = DODGE_DURATION;
        p.dodgeInvulnerable = true;
        p.stamina -= DODGE_STAMINA_COST;
        spentStaminaThisTick = true;
    } else if (in.block && p.stamina > 0.f) {
        p.state = ActionState::Blocking;
        p.stamina -= BLOCK_STAMINA_PER_SEC * dt;
        spentStaminaThisTick = true;

        Vec2 move{ in.moveX, in.moveY };
        if (move.length() > 0.01f) {
            move = move.normalized();
            p.pos = p.pos + move * (MOVE_SPEED * 0.5f * dt); // slower while blocking
            p.pos.x = clampf(p.pos.x, PLAYER_RADIUS, ARENA_WIDTH - PLAYER_RADIUS);
            p.pos.y = clampf(p.pos.y, PLAYER_RADIUS, ARENA_HEIGHT - PLAYER_RADIUS);
        }
    } else {
        Vec2 move{ in.moveX, in.moveY };
        if (move.length() > 0.01f) {
            move = move.normalized();
            p.pos = p.pos + move * (MOVE_SPEED * dt);
            p.pos.x = clampf(p.pos.x, PLAYER_RADIUS, ARENA_WIDTH - PLAYER_RADIUS);
            p.pos.y = clampf(p.pos.y, PLAYER_RADIUS, ARENA_HEIGHT - PLAYER_RADIUS);
            p.state = ActionState::Moving;
        } else {
            p.state = ActionState::Idle;
        }
    }

    if (p.stamina < 0.f) p.stamina = 0.f;

    // ---- Stamina regen -------------------------------------------------
    if (spentStaminaThisTick) {
        p.staminaRegenDelay = STAMINA_REGEN_DELAY;
    } else if (p.staminaRegenDelay > 0.f) {
        p.staminaRegenDelay -= dt;
    } else if (p.state != ActionState::Blocking) {
        p.stamina = clampf(p.stamina + STAMINA_REGEN_PER_SEC * dt, 0.f, MAX_STAMINA);
    }
}

void GameServer::resolveAttackHit(int attackerId) {
    ServerPlayer& attacker = players_[attackerId];
    ServerPlayer& defender = players_[1 - attackerId];

    attacker.attackHasHit = true; // this swing can only ever land once

    if (defender.state == ActionState::Dead) return;
    if (distance(attacker.pos, defender.pos) > ATTACK_RANGE) return;

    // Dodged clean - i-frames beat everything.
    if (defender.state == ActionState::Dodging && defender.dodgeInvulnerable) {
        return;
    }

    // Parried - the *attacker* gets punished, not the defender.
    if (defender.state == ActionState::Parrying) {
        float elapsed = (PARRY_WINDUP + PARRY_WINDOW + PARRY_RECOVERY) - defender.actionTimer;
        bool parryIsLive = elapsed >= PARRY_WINDUP && elapsed < (PARRY_WINDUP + PARRY_WINDOW);
        if (parryIsLive) {
            resolveParryOutcome(1 - attackerId);
            return;
        }
    }

    float damage = ATTACK_DAMAGE;
    bool causesStagger = true;

    if (defender.state == ActionState::Blocking) {
        damage *= BLOCK_DAMAGE_MULT;
        causesStagger = false; // a held block absorbs the hit without staggering
    }

    defender.health = clampf(defender.health - damage, 0.f, MAX_HEALTH);

    if (defender.health <= 0.f) {
        defender.state = ActionState::Dead;
        winnerId_ = static_cast<uint8_t>(attackerId);
        return;
    }

    if (causesStagger) {
        defender.state = ActionState::Staggered;
        defender.actionTimer = HIT_STAGGER_DURATION;
    }
}

void GameServer::resolveParryOutcome(int defenderId) {
    // defenderId just successfully parried - stagger the attacker instead.
    ServerPlayer& attacker = players_[1 - defenderId];
    attacker.state = ActionState::Staggered;
    attacker.actionTimer = PARRY_STAGGER_DURATION;
}

// -----------------------------------------------------------------------
// Broadcast
// -----------------------------------------------------------------------

void GameServer::broadcastState() {
    StateUpdatePacket update;
    update.tick = tickCounter_;
    update.winnerId = winnerId_;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        PlayerSnapshot& snap = update.players[i];
        snap.x = players_[i].pos.x;
        snap.y = players_[i].pos.y;
        snap.facing = players_[i].facing;
        snap.health = players_[i].health;
        snap.stamina = players_[i].stamina;
        snap.state = players_[i].state;
        snap.connected = players_[i].connected ? 1 : 0;
    }

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (players_[i].connected) {
            sock_.send(&update, sizeof(update), players_[i].endpoint);
        }
    }
}
