// Fighter in isolation. These tests exist because Fighter promises things
// about its own state - health stays in range, the dead stay dead, a swing
// only counts once - and those promises are the reason its data is private.

#include "server/sim/Fighter.h"
#include "Check.h"

using namespace net;

namespace {

Fighter spawned() {
    Fighter f;
    f.spawnAt({ ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f }, 1.f);
    return f;
}

void run(Fighter& f, float seconds) {
    int steps = static_cast<int>(seconds / TICK_DT);
    for (int i = 0; i < steps; ++i) f.update(TICK_DT);
}

PlayerInput attackPress() {
    PlayerInput in;
    in.attack = true;
    return in;
}

} // namespace

int main() {
    TEST("a new fighter is active and at full health");
    {
        Fighter f = spawned();
        CHECK(f.isActive());
        CHECK_NEAR(f.health(), MAX_HEALTH);
        CHECK_NEAR(f.stamina(), MAX_STAMINA);
        CHECK(f.state() == ActionState::Idle);
        CHECK(!f.isDead());
    }

    TEST("health cannot go below zero");
    {
        Fighter f = spawned();
        f.takeDamage(MAX_HEALTH * 10.f);
        CHECK_NEAR(f.health(), 0.f);
        CHECK(f.isDead());
    }

    TEST("the dead stay dead and cannot be hurt or staggered again");
    {
        Fighter f = spawned();
        f.takeDamage(MAX_HEALTH);
        CHECK(f.isDead());

        f.takeDamage(50.f);
        CHECK_NEAR(f.health(), 0.f);
        CHECK(f.state() == ActionState::Dead);

        f.stagger(1.f);
        CHECK(f.state() == ActionState::Dead);

        f.setInput(attackPress());
        run(f, 1.f);
        CHECK(f.state() == ActionState::Dead);
    }

    TEST("stamina never goes negative");
    {
        Fighter f = spawned();
        for (int i = 0; i < 20; ++i) {
            f.setInput(attackPress());
            run(f, ATTACK_WINDUP + ATTACK_ACTIVE + ATTACK_RECOVERY + 0.02f);
            CHECK(f.stamina() >= 0.f);
        }
    }

    TEST("a fighter cannot be pushed out of the arena");
    {
        Fighter f = spawned();
        f.nudge({ -10000.f, -10000.f });
        CHECK_NEAR(f.position().x, PLAYER_RADIUS);
        CHECK_NEAR(f.position().y, PLAYER_RADIUS);

        f.nudge({ 10000.f, 10000.f });
        CHECK_NEAR(f.position().x, ARENA_WIDTH - PLAYER_RADIUS);
        CHECK_NEAR(f.position().y, ARENA_HEIGHT - PLAYER_RADIUS);
    }

    TEST("a swing is only live during its active window");
    {
        Fighter f = spawned();
        f.setInput(attackPress());
        f.update(TICK_DT);                       // the attack starts here
        CHECK(f.state() == ActionState::Attacking);
        CHECK(!f.attackCanLand());               // still winding up

        bool wasLive = false;
        int liveTicks = 0;
        for (int i = 0; i < 40; ++i) {
            f.update(TICK_DT);
            if (f.attackCanLand()) { wasLive = true; ++liveTicks; }
        }
        CHECK(wasLive);
        CHECK(liveTicks > 1);                    // a real window, not one tick
        CHECK(!f.attackCanLand());               // and it closes again
    }

    TEST("marking a swing spent closes it early");
    {
        Fighter f = spawned();
        f.setInput(attackPress());

        // Step until the window opens rather than guessing the exact tick.
        int guard = 0;
        while (!f.attackCanLand() && guard++ < 60) f.update(TICK_DT);
        CHECK(f.attackCanLand());

        f.markAttackSpent();
        CHECK(!f.attackCanLand());
        run(f, ATTACK_ACTIVE);
        CHECK(!f.attackCanLand());               // stays spent for this swing
    }

    TEST("a parry is only live during its window");
    {
        Fighter f = spawned();
        PlayerInput in;
        in.parry = true;
        f.setInput(in);
        f.update(TICK_DT);
        CHECK(f.state() == ActionState::Parrying);
        CHECK(!f.isParryLive());                 // winding up

        int liveTicks = 0;
        for (int i = 0; i < 40; ++i) {
            f.update(TICK_DT);
            if (f.isParryLive()) ++liveTicks;
        }
        CHECK(liveTicks > 1);
        CHECK(!f.isParryLive());
    }

    TEST("dodge invulnerability is shorter than the dodge itself");
    {
        Fighter f = spawned();
        PlayerInput in;
        in.dodge = true;
        f.setInput(in);
        f.update(TICK_DT);
        CHECK(f.state() == ActionState::Dodging);
        CHECK(f.isInvulnerable());

        run(f, DODGE_IFRAME_TIME + 0.01f);
        CHECK(!f.isInvulnerable());
    }

    TEST("leaving resets the fighter completely");
    {
        Fighter f = spawned();
        f.takeDamage(30.f);
        f.leave();
        CHECK(!f.isActive());
        CHECK_NEAR(f.health(), MAX_HEALTH);
    }

    TEST("a snapshot reports what the fighter says it is");
    {
        Fighter f = spawned();
        f.takeDamage(ATTACK_DAMAGE);

        PlayerSnapshot snap;
        f.writeSnapshot(snap);
        CHECK_NEAR(snap.health, f.health());
        CHECK_NEAR(snap.stamina, f.stamina());
        CHECK_NEAR(snap.x, f.position().x);
        CHECK(snap.state == f.state());
        CHECK(snap.connected == 1);
    }

    return testSummary("fighter_test");
}
