// Combat rules, checked directly against ArenaSim. No sockets, no threads,
// no sleeping - everything here is a pure function of the inputs we feed in.

#include "server/sim/ArenaSim.h"
#include "Check.h"

using namespace net;

namespace {

// A sim with both players in, facing each other at the given gap.
ArenaSim duel(float gap) {
    ArenaSim sim;
    sim.addPlayer(0);
    sim.addPlayer(1);
    float mid = ARENA_WIDTH / 2.f;
    sim.setPosition(0, { mid - gap / 2.f, ARENA_HEIGHT / 2.f });
    sim.setPosition(1, { mid + gap / 2.f, ARENA_HEIGHT / 2.f });
    return sim;
}

void run(ArenaSim& sim, float seconds) {
    int steps = static_cast<int>(seconds / TICK_DT);
    for (int i = 0; i < steps; ++i) sim.step(TICK_DT);
}

PlayerInput press(bool attack = false, bool block = false,
                  bool parry = false, bool dodge = false) {
    PlayerInput in;
    in.attack = attack;
    in.block = block;
    in.parry = parry;
    in.dodge = dodge;
    return in;
}

PlayerInput move(float x, float y = 0.f) {
    PlayerInput in;
    in.moveX = x;
    in.moveY = y;
    return in;
}

float gapBetween(const ArenaSim& sim) {
    return distance(sim.player(0).position(), sim.player(1).position());
}

} // namespace

int main() {
    TEST("attack in range deals damage");
    {
        ArenaSim sim = duel(50.f);
        sim.setInput(0, press(true));
        run(sim, 0.5f);
        CHECK_NEAR(sim.player(1).health(), MAX_HEALTH - ATTACK_DAMAGE);
        CHECK_NEAR(sim.player(0).stamina(), MAX_STAMINA - ATTACK_STAMINA_COST);
    }

    TEST("attack out of range does nothing");
    {
        ArenaSim sim = duel(200.f);
        sim.setInput(0, press(true));
        run(sim, 0.5f);
        CHECK_NEAR(sim.player(1).health(), MAX_HEALTH);
    }

    TEST("defender walking into range mid-swing still gets hit");
    {
        // Regression: the active window used to be consumed by the first
        // whiffing tick, so this swing missed no matter how close they got.
        ArenaSim sim = duel(95.f);
        sim.setInput(0, press(true));
        for (int i = 0; i < 30; ++i) {
            sim.setInput(1, move(-1.f)); // player 1 runs at player 0
            sim.step(TICK_DT);
        }
        CHECK(sim.player(1).health() < MAX_HEALTH);
        CHECK(gapBetween(sim) < ATTACK_RANGE);
    }

    TEST("one swing only lands once");
    {
        ArenaSim sim = duel(50.f);
        sim.setInput(0, press(true));
        run(sim, 0.5f);
        CHECK_NEAR(sim.player(1).health(), MAX_HEALTH - ATTACK_DAMAGE);
    }

    TEST("blocking reduces damage and avoids stagger");
    {
        ArenaSim sim = duel(50.f);
        sim.setInput(1, press(false, true));
        sim.step(TICK_DT);
        sim.setInput(0, press(true));
        for (int i = 0; i < 12; ++i) {  // hold block through the swing
            sim.setInput(1, press(false, true));
            sim.step(TICK_DT);
        }
        CHECK_NEAR(sim.player(1).health(), MAX_HEALTH - ATTACK_DAMAGE * BLOCK_DAMAGE_MULT);
        CHECK(sim.player(1).state() != ActionState::Staggered);
    }

    TEST("parry staggers the attacker and blocks all damage");
    {
        ArenaSim sim = duel(50.f);
        sim.setInput(1, press(false, false, true));
        sim.step(TICK_DT);                  // parry starts here
        sim.setInput(0, press(true));
        run(sim, 0.25f);                    // swing lands inside the live window
        CHECK_NEAR(sim.player(1).health(), MAX_HEALTH);
        CHECK(sim.player(0).state() == ActionState::Staggered);
    }

    TEST("dodge i-frames avoid the hit");
    {
        ArenaSim sim = duel(50.f);
        sim.setInput(0, press(true));
        run(sim, ATTACK_WINDUP - 0.02f);
        sim.setInput(1, press(false, false, false, true));
        run(sim, 0.4f);
        CHECK_NEAR(sim.player(1).health(), MAX_HEALTH);
    }

    TEST("stamina blocks actions and regenerates after a delay");
    {
        ArenaSim sim = duel(300.f);
        // Burn stamina down with repeated swings.
        for (int i = 0; i < 8; ++i) {
            sim.setInput(0, press(true));
            run(sim, ATTACK_WINDUP + ATTACK_ACTIVE + ATTACK_RECOVERY + 0.02f);
        }
        CHECK(sim.player(0).stamina() < ATTACK_STAMINA_COST);

        float before = sim.player(0).stamina();
        run(sim, STAMINA_REGEN_DELAY + 1.f);
        CHECK(sim.player(0).stamina() > before);
    }

    TEST("a press during stagger fires once the player recovers");
    {
        ArenaSim sim = duel(50.f);
        sim.setInput(0, press(true));
        run(sim, 0.3f);
        CHECK(sim.player(1).state() == ActionState::Staggered);

        sim.setInput(1, press(true));      // buffered while staggered
        run(sim, HIT_STAGGER_DURATION + 0.3f);
        CHECK(sim.player(0).health() < MAX_HEALTH);
    }

    TEST("lethal damage ends the match and freezes it");
    {
        ArenaSim sim = duel(50.f);
        for (int i = 0; i < 40 && !sim.matchOver(); ++i) {
            sim.setInput(0, press(true));
            run(sim, 1.2f);   // long enough to regain stamina between swings
        }
        CHECK(sim.matchOver());
        CHECK(sim.winner() == 0);
        CHECK(sim.player(1).state() == ActionState::Dead);

        Vec2 posBefore = sim.player(0).position();
        sim.setInput(0, move(1.f));
        run(sim, 0.5f);
        CHECK_NEAR(sim.player(0).position().x, posBefore.x);
    }

    TEST("resetMatch restores health and clears the winner");
    {
        ArenaSim sim = duel(50.f);
        for (int i = 0; i < 40 && !sim.matchOver(); ++i) {
            sim.setInput(0, press(true));
            run(sim, 1.2f);
        }
        CHECK(sim.matchOver());

        sim.resetMatch();
        CHECK(!sim.matchOver());
        CHECK(sim.winner() == NO_WINNER);
        CHECK_NEAR(sim.player(0).health(), MAX_HEALTH);
        CHECK_NEAR(sim.player(1).health(), MAX_HEALTH);
        CHECK(sim.active(0) && sim.active(1));
    }

    TEST("players cannot overlap or leave the arena");
    {
        ArenaSim sim = duel(60.f);
        for (int i = 0; i < 200; ++i) {
            sim.setInput(0, move(1.f));
            sim.setInput(1, move(-1.f));
            sim.step(TICK_DT);
        }
        CHECK(gapBetween(sim) >= PLAYER_RADIUS * 2.f - 0.5f);

        for (int i = 0; i < 600; ++i) {
            sim.setInput(0, move(-1.f, -1.f));
            sim.step(TICK_DT);
        }
        CHECK(sim.player(0).position().x >= PLAYER_RADIUS - 0.01f);
        CHECK(sim.player(0).position().y >= PLAYER_RADIUS - 0.01f);
    }

    TEST("stepping with one player present is harmless");
    {
        ArenaSim sim;
        sim.addPlayer(0);
        sim.setInput(0, press(true));
        run(sim, 1.f);
        CHECK(!sim.matchOver());
        CHECK(sim.activeCount() == 1);
    }

    return testSummary("sim_test");
}
