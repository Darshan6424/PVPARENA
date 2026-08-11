#include "client/app/MatchWatcher.h"

using namespace net;

void MatchWatcher::reset() {
    prev_ = {};
    stateTime_ = {};
    muteAttack_ = {};
    havePrev_ = false;
    endReported_ = false;
}

void MatchWatcher::muteNextAttackSound(int playerId) {
    if (playerId >= 0 && playerId < MAX_PLAYERS) muteAttack_[playerId] = true;
}

MatchEvents MatchWatcher::observe(const StateUpdatePacket& state, float dt) {
    MatchEvents events;

    if (!havePrev_) {
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            prev_[i] = state.players[i];
            stateTime_[i] = 0.f;
        }
        havePrev_ = true;
        return events;
    }

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const PlayerSnapshot& now = state.players[i];
        const PlayerSnapshot& was = prev_[i];
        PlayerEvents& out = events.players[i];

        if (now.state != was.state) {
            stateTime_[i] = 0.f;
        } else {
            stateTime_[i] += dt;
        }

        if (now.state == ActionState::Attacking && was.state != ActionState::Attacking) {
            if (muteAttack_[i]) {
                muteAttack_[i] = false;
            } else {
                out.startedAttack = true;
            }
        }

        // Swung into a live parry: you were attacking, now you're staggered,
        // and the other player was parrying a moment ago.
        if (now.state == ActionState::Staggered &&
            was.state == ActionState::Attacking &&
            prev_[1 - i].state == ActionState::Parrying) {
            out.gotParried = true;
        }

        if (now.health < was.health - 0.01f) {
            bool blocked = now.state == ActionState::Blocking ||
                           was.state == ActionState::Blocking;
            out.tookCleanHit = !blocked;
            out.tookBlockedHit = blocked;
            out.hitX = now.x;
            out.hitY = now.y;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        prev_[i] = state.players[i];
    }

    if (state.winnerId != NO_WINNER && !endReported_) {
        events.matchJustEnded = true;
        endReported_ = true;
    }

    return events;
}
