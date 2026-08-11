#pragma once
// Compares each snapshot with the one before it and says what just happened,
// so the audio and effects code doesn't have to diff packets itself.
//
// Deliberately knows nothing about SFML - it's plain data in, plain data out.

#include "common/Protocol.h"
#include <array>

struct PlayerEvents {
    bool startedAttack = false;
    bool gotParried = false;      // was mid-swing and got staggered by a parry
    bool tookCleanHit = false;
    bool tookBlockedHit = false;
    float damage = 0.f;       // how much health this hit actually removed
    float hitX = 0.f;
    float hitY = 0.f;
};

struct MatchEvents {
    std::array<PlayerEvents, net::MAX_PLAYERS> players;
    bool matchJustEnded = false;
};

class MatchWatcher {
public:
    void reset();

    // Call once per frame with the newest snapshot.
    MatchEvents observe(const net::StateUpdatePacket& state, float dt);

    // Seconds this player has been in its current state. Drives animation.
    float stateTime(int playerId) const { return stateTime_[playerId]; }

    // The local player's swing sound already played the moment they pressed
    // the button, so don't play it again when the server confirms.
    void muteNextAttackSound(int playerId);

private:
    std::array<net::PlayerSnapshot, net::MAX_PLAYERS> prev_{};
    std::array<float, net::MAX_PLAYERS> stateTime_{};
    std::array<bool, net::MAX_PLAYERS> muteAttack_{};
    bool havePrev_ = false;
    bool endReported_ = false;
};
