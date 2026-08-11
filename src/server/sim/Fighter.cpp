#include "server/sim/Fighter.h"

using namespace net;

namespace {

constexpr float ATTACK_TOTAL = ATTACK_WINDUP + ATTACK_ACTIVE + ATTACK_RECOVERY;
constexpr float PARRY_TOTAL = PARRY_WINDUP + PARRY_WINDOW + PARRY_RECOVERY;

bool lockedState(ActionState s) {
    return s == ActionState::Attacking || s == ActionState::Parrying ||
           s == ActionState::Dodging   || s == ActionState::Staggered;
}

// How far into the current action we are.
float elapsed(float length, float remaining) {
    return length - remaining;
}

} // namespace

void Fighter::spawnAt(Vec2 pos, float facing) {
    active_ = true;
    pos_ = pos;
    facing_ = facing;
    health_ = MAX_HEALTH;
    stamina_ = MAX_STAMINA;
    state_ = ActionState::Idle;
    actionTimer_ = 0.f;
    actionLength_ = 0.f;
    regenDelay_ = 0.f;
    attackConnected_ = false;
    dodgeInvulnerable_ = false;
    input_ = PlayerInput{};
}

void Fighter::leave() {
    *this = Fighter{};
}

void Fighter::setInput(const PlayerInput& in) {
    input_.moveX = in.moveX;
    input_.moveY = in.moveY;
    input_.block = in.block;
    input_.attack = input_.attack || in.attack;
    input_.parry = input_.parry || in.parry;
    input_.dodge = input_.dodge || in.dodge;
}

void Fighter::faceToward(Vec2 target) {
    if (target.x != pos_.x) {
        facing_ = (target.x > pos_.x) ? 1.f : -1.f;
    }
}

bool Fighter::isLocked() const {
    return lockedState(state_);
}

bool Fighter::isInvulnerable() const {
    return state_ == ActionState::Dodging && dodgeInvulnerable_;
}

bool Fighter::isParryLive() const {
    if (state_ != ActionState::Parrying) return false;
    float t = elapsed(actionLength_, actionTimer_);
    return t >= PARRY_WINDUP && t < PARRY_WINDUP + PARRY_WINDOW;
}

bool Fighter::attackCanLand() const {
    if (state_ != ActionState::Attacking || attackConnected_) return false;
    float t = elapsed(actionLength_, actionTimer_);
    return t >= ATTACK_WINDUP && t < ATTACK_WINDUP + ATTACK_ACTIVE;
}

void Fighter::update(float dt) {
    if (isDead()) return;

    if (isLocked()) {
        advanceCurrentAction(dt);
        return;
    }

    bool spent = startQueuedAction();

    if (!isLocked()) {
        // Nothing started, so this is a walking, blocking or standing tick.
        Vec2 move{ input_.moveX, input_.moveY };
        bool moving = move.length() > 0.01f;
        bool blocking = input_.block && stamina_ > 0.f;

        if (blocking) {
            state_ = ActionState::Blocking;
            stamina_ -= BLOCK_STAMINA_PER_SEC * dt;
            spent = true;
        } else {
            state_ = moving ? ActionState::Moving : ActionState::Idle;
        }

        if (moving) {
            walk(move, blocking ? MOVE_SPEED * 0.5f : MOVE_SPEED, dt);
        }
    }

    if (stamina_ < 0.f) stamina_ = 0.f;
    regenerate(dt, spent);
}

void Fighter::advanceCurrentAction(float dt) {
    actionTimer_ -= dt;

    if (state_ == ActionState::Dodging) {
        Vec2 dir{ input_.moveX, input_.moveY };
        if (dir.length() < 0.01f) dir = { facing_, 0.f };
        pos_ += dir.normalized() * (DODGE_SPEED * dt);
        clampInsideArena();

        dodgeInvulnerable_ = elapsed(actionLength_, actionTimer_) < DODGE_IFRAME_TIME;
    }

    if (actionTimer_ <= 0.f) {
        state_ = ActionState::Idle;
        dodgeInvulnerable_ = false;
    }
}

bool Fighter::startQueuedAction() {
    // Consume the remembered presses so each one only ever fires once.
    bool wantAttack = input_.attack;
    bool wantParry = input_.parry;
    bool wantDodge = input_.dodge;
    input_.attack = false;
    input_.parry = false;
    input_.dodge = false;

    if (wantAttack && stamina_ >= ATTACK_STAMINA_COST) {
        beginAction(ActionState::Attacking, ATTACK_TOTAL, ATTACK_STAMINA_COST);
        attackConnected_ = false;
        return true;
    }
    if (wantParry && stamina_ >= PARRY_STAMINA_COST) {
        beginAction(ActionState::Parrying, PARRY_TOTAL, PARRY_STAMINA_COST);
        return true;
    }
    if (wantDodge && stamina_ >= DODGE_STAMINA_COST) {
        beginAction(ActionState::Dodging, DODGE_DURATION, DODGE_STAMINA_COST);
        dodgeInvulnerable_ = true;
        return true;
    }
    return false;
}

void Fighter::beginAction(ActionState state, float duration, float staminaCost) {
    state_ = state;
    actionLength_ = duration;
    actionTimer_ = duration;
    stamina_ -= staminaCost;
}

void Fighter::walk(const Vec2& direction, float speed, float dt) {
    pos_ += direction.normalized() * (speed * dt);
    clampInsideArena();
}

void Fighter::regenerate(float dt, bool spentThisTick) {
    if (spentThisTick) {
        regenDelay_ = STAMINA_REGEN_DELAY;
    } else if (regenDelay_ > 0.f) {
        regenDelay_ -= dt;
    } else {
        stamina_ = clampf(stamina_ + STAMINA_REGEN_PER_SEC * dt, 0.f, MAX_STAMINA);
    }
}

void Fighter::takeDamage(float amount) {
    if (isDead()) return;

    health_ = clampf(health_ - amount, 0.f, MAX_HEALTH);
    if (health_ <= 0.f) {
        state_ = ActionState::Dead;
        actionTimer_ = 0.f;
        dodgeInvulnerable_ = false;
    }
}

void Fighter::stagger(float duration) {
    if (isDead()) return;
    state_ = ActionState::Staggered;
    actionLength_ = duration;
    actionTimer_ = duration;
    dodgeInvulnerable_ = false;
}

void Fighter::nudge(Vec2 delta) {
    pos_ += delta;
    clampInsideArena();
}

void Fighter::clampInsideArena() {
    pos_.x = clampf(pos_.x, PLAYER_RADIUS, ARENA_WIDTH - PLAYER_RADIUS);
    pos_.y = clampf(pos_.y, PLAYER_RADIUS, ARENA_HEIGHT - PLAYER_RADIUS);
}

void Fighter::writeSnapshot(PlayerSnapshot& out) const {
    out.x = pos_.x;
    out.y = pos_.y;
    out.facing = facing_;
    out.health = health_;
    out.stamina = stamina_;
    out.state = state_;
    out.connected = active_ ? 1 : 0;
}
