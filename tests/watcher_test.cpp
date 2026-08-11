// MatchWatcher turns consecutive snapshots into "what just happened". It has
// no SFML in it, so it can be tested like any other pure function.

#include "client/app/MatchWatcher.h"
#include "Check.h"

using namespace net;

namespace {

StateUpdatePacket freshState() {
    StateUpdatePacket s;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        s.players[i].connected = 1;
        s.players[i].health = MAX_HEALTH;
        s.players[i].state = ActionState::Idle;
    }
    return s;
}

} // namespace

int main() {
    TEST("the first snapshot reports nothing");
    {
        MatchWatcher w;
        MatchEvents e = w.observe(freshState(), 0.016f);
        CHECK(!e.players[0].startedAttack);
        CHECK(!e.matchJustEnded);
    }

    TEST("an attack starting is reported once");
    {
        MatchWatcher w;
        StateUpdatePacket s = freshState();
        w.observe(s, 0.016f);

        s.players[0].state = ActionState::Attacking;
        CHECK(w.observe(s, 0.016f).players[0].startedAttack);
        CHECK(!w.observe(s, 0.016f).players[0].startedAttack);
    }

    TEST("a muted swing is swallowed exactly once");
    {
        MatchWatcher w;
        StateUpdatePacket s = freshState();
        w.observe(s, 0.016f);
        w.muteNextAttackSound(0);

        s.players[0].state = ActionState::Attacking;
        CHECK(!w.observe(s, 0.016f).players[0].startedAttack);

        s.players[0].state = ActionState::Idle;
        w.observe(s, 0.016f);
        s.players[0].state = ActionState::Attacking;
        CHECK(w.observe(s, 0.016f).players[0].startedAttack);
    }

    TEST("a clean hit reports position, a blocked one is flagged separately");
    {
        MatchWatcher w;
        StateUpdatePacket s = freshState();
        w.observe(s, 0.016f);

        s.players[1].health -= ATTACK_DAMAGE;
        s.players[1].x = 123.f;
        s.players[1].y = 45.f;
        MatchEvents e = w.observe(s, 0.016f);
        CHECK(e.players[1].tookCleanHit);
        CHECK(!e.players[1].tookBlockedHit);
        CHECK_NEAR(e.players[1].damage, ATTACK_DAMAGE);
        CHECK_NEAR(e.players[1].hitX, 123.f);
        CHECK_NEAR(e.players[1].hitY, 45.f);

        s.players[1].state = ActionState::Blocking;
        w.observe(s, 0.016f);
        s.players[1].health -= 5.f;
        MatchEvents blocked = w.observe(s, 0.016f);
        CHECK(blocked.players[1].tookBlockedHit);
        CHECK(!blocked.players[1].tookCleanHit);
        CHECK_NEAR(blocked.players[1].damage, 5.f);
    }

    TEST("swinging into a parry is reported");
    {
        MatchWatcher w;
        StateUpdatePacket s = freshState();
        s.players[0].state = ActionState::Attacking;
        s.players[1].state = ActionState::Parrying;
        w.observe(s, 0.016f);

        s.players[0].state = ActionState::Staggered;
        CHECK(w.observe(s, 0.016f).players[0].gotParried);
    }

    TEST("the end of the match is announced once");
    {
        MatchWatcher w;
        StateUpdatePacket s = freshState();
        w.observe(s, 0.016f);

        s.winnerId = 0;
        CHECK(w.observe(s, 0.016f).matchJustEnded);
        CHECK(!w.observe(s, 0.016f).matchJustEnded);
    }

    TEST("state time accumulates and resets when the state changes");
    {
        MatchWatcher w;
        StateUpdatePacket s = freshState();
        w.observe(s, 0.1f);
        w.observe(s, 0.1f);
        w.observe(s, 0.1f);
        CHECK_NEAR(w.stateTime(0), 0.2f);

        s.players[0].state = ActionState::Moving;
        w.observe(s, 0.1f);
        CHECK_NEAR(w.stateTime(0), 0.f);
    }

    return testSummary("watcher_test");
}
