#pragma once
// One duellist. Owns its own health, stamina and action state, and is the only
// thing allowed to change them.
//
// Everything here is private because it all has rules attached: health never
// leaves 0..MAX_HEALTH, a fighter at zero health is Dead and stays Dead, an
// action timer only runs while an action is in progress, and a swing can only
// deal damage once. Callers ask questions (canAct, isParryLive) and give
// orders (takeDamage, stagger); they cannot put the fighter into a state the
// rules forbid.
//
// Anything involving two fighters - who hit whom, pushing them apart - is not
// here. That is the arena's job. See ArenaSim.

#include "common/Protocol.h"
#include "common/MathUtils.h"

struct PlayerInput {
    float moveX = 0.f;
    float moveY = 0.f;
    bool attack = false;
    bool block = false;
    bool parry = false;
    bool dodge = false;
};

class Fighter {
public:
    void spawnAt(Vec2 pos, float facing);
    void leave();
    bool isActive() const { return active_; }

    // Movement and block are replaced. Attack, parry and dodge are remembered
    // until they are used, so a press during a stagger still comes out when
    // the fighter recovers.
    void setInput(const PlayerInput& in);

    void faceToward(Vec2 target);

    // Advance one tick: run down the action timer, start a new action if one
    // is waiting, move, and regenerate stamina.
    void update(float dt);

    net::ActionState state() const { return state_; }
    Vec2 position() const { return pos_; }
    float facing() const { return facing_; }
    float health() const { return health_; }
    float stamina() const { return stamina_; }

    bool isDead() const { return state_ == net::ActionState::Dead; }
    bool isBlocking() const { return state_ == net::ActionState::Blocking; }

    // True only during the invulnerable part of a dodge.
    bool isInvulnerable() const;
    // True only while a parry would actually catch a swing.
    bool isParryLive() const;
    // True while this swing is in its active window and has not connected yet.
    bool attackCanLand() const;

    void markAttackSpent() { attackConnected_ = true; }
    void takeDamage(float amount);
    void stagger(float duration);
    void nudge(Vec2 delta);

    void writeSnapshot(net::PlayerSnapshot& out) const;

private:
    bool isLocked() const;
    void beginAction(net::ActionState state, float duration, float staminaCost);
    void advanceCurrentAction(float dt);
    bool startQueuedAction();
    void walk(const Vec2& direction, float speed, float dt);
    void regenerate(float dt, bool spentThisTick);
    void clampInsideArena();

    bool active_ = false;

    Vec2 pos_;
    float facing_ = 1.f;
    float health_ = net::MAX_HEALTH;
    float stamina_ = net::MAX_STAMINA;

    net::ActionState state_ = net::ActionState::Idle;
    float actionTimer_ = 0.f;
    float actionLength_ = 0.f;
    float regenDelay_ = 0.f;

    bool attackConnected_ = false;
    bool dodgeInvulnerable_ = false;

    PlayerInput input_;
};
