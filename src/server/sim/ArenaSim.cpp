#include "server/sim/ArenaSim.h"

using namespace net;

namespace {

Vec2 spawnPos(int id) {
    return (id == 0) ? Vec2{ 120.f, ARENA_HEIGHT / 2.f }
                     : Vec2{ ARENA_WIDTH - 120.f, ARENA_HEIGHT / 2.f };
}

float spawnFacing(int id) {
    return (id == 0) ? 1.f : -1.f;
}

} // namespace

void ArenaSim::resetMatch() {
    winner_ = NO_WINNER;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (fighters_[i].isActive()) {
            fighters_[i].spawnAt(spawnPos(i), spawnFacing(i));
        }
    }
}

void ArenaSim::addPlayer(int id) {
    fighters_[id].spawnAt(spawnPos(id), spawnFacing(id));
}

void ArenaSim::removePlayer(int id) {
    fighters_[id].leave();
}

bool ArenaSim::active(int id) const {
    return fighters_[id].isActive();
}

int ArenaSim::activeCount() const {
    int n = 0;
    for (const Fighter& f : fighters_) {
        if (f.isActive()) ++n;
    }
    return n;
}

void ArenaSim::setInput(int id, const PlayerInput& in) {
    fighters_[id].setInput(in);
}

void ArenaSim::setPosition(int id, Vec2 pos) {
    fighters_[id].nudge(pos - fighters_[id].position());
}

void ArenaSim::step(float dt) {
    if (matchOver()) return;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        Fighter& f = fighters_[i];
        if (!f.isActive()) continue;

        const Fighter& other = fighters_[1 - i];
        if (other.isActive()) f.faceToward(other.position());
        f.update(dt);
    }

    // Both fighters move first, then swings are resolved. Doing it this way
    // means a double knockout is a real trade instead of player 0 always
    // winning the tie by being updated first.
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (fighters_[i].isActive() && fighters_[i].attackCanLand()) {
            resolveAttack(i);
        }
    }

    separateFighters();
}

void ArenaSim::resolveAttack(int attackerId) {
    Fighter& attacker = fighters_[attackerId];
    Fighter& defender = fighters_[1 - attackerId];

    // Returning without marking the swing spent leaves the rest of the active
    // window live, so a defender who walks into range mid-swing still gets hit.
    if (!defender.isActive() || defender.isDead()) return;
    if (distance(attacker.position(), defender.position()) > ATTACK_RANGE) return;
    if (defender.isInvulnerable()) return;

    attacker.markAttackSpent();

    if (defender.isParryLive()) {
        attacker.stagger(PARRY_STAGGER_DURATION);
        return;
    }

    bool blocked = defender.isBlocking();
    defender.takeDamage(blocked ? ATTACK_DAMAGE * BLOCK_DAMAGE_MULT : ATTACK_DAMAGE);

    if (defender.isDead()) {
        winner_ = static_cast<uint8_t>(attackerId);
        return;
    }

    // A held block absorbs the hit without breaking your stance.
    if (!blocked) {
        defender.stagger(HIT_STAGGER_DURATION);
    }
}

void ArenaSim::separateFighters() {
    Fighter& a = fighters_[0];
    Fighter& b = fighters_[1];
    if (!a.isActive() || !b.isActive()) return;
    if (a.isDead() || b.isDead()) return;

    Vec2 delta = b.position() - a.position();
    float dist = delta.length();
    const float minGap = PLAYER_RADIUS * 2.f;

    if (dist > 1e-4f && dist < minGap) {
        Vec2 push = delta.normalized() * ((minGap - dist) * 0.5f);
        a.nudge(-push);
        b.nudge(push);
    } else if (dist <= 1e-4f) {
        a.nudge({ -minGap * 0.5f, 0.f });
        b.nudge({ minGap * 0.5f, 0.f });
    }
}

void ArenaSim::fillSnapshots(PlayerSnapshot* out) const {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        fighters_[i].writeSnapshot(out[i]);
    }
}
