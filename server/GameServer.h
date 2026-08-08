#pragma once
// GameServer.h
// The authoritative simulation. Owns both players' true state, applies
// inputs, resolves combat, and broadcasts StateUpdate packets every tick.
// The client NEVER computes gameplay - it only renders what this produces.

#include "../common/Protocol.h"
#include "../common/UdpSocket.h"
#include "../common/MathUtils.h"
#include <atomic>
#include <array>

struct ServerPlayer {
    bool     connected = false;
    Endpoint endpoint;

    Vec2  pos;
    float facing = 1.f;
    float health  = net::MAX_HEALTH;
    float stamina = net::MAX_STAMINA;

    net::ActionState state = net::ActionState::Idle;
    float actionTimer = 0.f;   // counts down through windup/active/recovery/stagger
    float staminaRegenDelay = 0.f;

    // Whether the *current* attack has already landed a hit (so a single
    // swing can't hit twice while active).
    bool attackHasHit = false;
    // Whether the current dodge is still in its i-frame window.
    bool dodgeInvulnerable = false;

    net::InputStatePacket lastInput{};
    bool hadInputThisTick = false;
};

class GameServer {
public:
    // Starts listening on `port`. `running` lets the caller (e.g. the
    // client's "Create Server" button) stop the server from another
    // thread by setting it to false.
    bool start(uint16_t port);

    // Blocking call - runs the fixed-tick loop until `keepRunning`
    // becomes false. Intended to be called on its own thread.
    void run(std::atomic<bool>& keepRunning);

    void stop();

private:
    void handleIncomingPackets();
    void handleConnectRequest(const Endpoint& from);
    void handleInputPacket(const net::InputStatePacket& pkt);

    void tick(float dt);
    void updatePlayer(int id, float dt);
    void resolvePlayerCollision();
    void resolveAttackHit(int attackerId);
    void resolveParryOutcome(int defenderId);

    void broadcastState();

    UdpSocket sock_;
    std::array<ServerPlayer, net::MAX_PLAYERS> players_;
    int connectedCount_ = 0;
    uint32_t tickCounter_ = 0;
    uint8_t winnerId_ = 255;
};
