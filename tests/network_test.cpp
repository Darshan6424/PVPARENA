// Handshake, session ownership and reconnect, over a real UDP socket but with
// the server pumped by hand so there are no threads and nothing to wait on.

#include "server/net/GameServer.h"
#include "client/net/GameClient.h"
#include "common/Protocol.h"
#include "Check.h"

using namespace net;

namespace {

constexpr uint16_t PORT = 9711;

// One "frame" for everyone: clients talk, server reads, ticks and replies,
// then clients read the reply.
void pump(GameServer& server, GameClient* a, GameClient* b, int frames = 4) {
    for (int i = 0; i < frames; ++i) {
        if (a) a->update(TICK_DT);
        if (b) b->update(TICK_DT);
        server.pumpNetwork();
        server.advance(TICK_DT);
        server.broadcastState();
        if (a) a->update(TICK_DT);
        if (b) b->update(TICK_DT);
    }
}

} // namespace

int main() {
    TEST("two clients connect and get distinct ids");
    GameServer server;
    if (!server.start(PORT)) {
        std::printf("could not bind test port %u\n", PORT);
        return 1;
    }

    GameClient a, b;
    CHECK(a.beginConnect("127.0.0.1", PORT));
    CHECK(b.beginConnect("127.0.0.1", PORT));
    pump(server, &a, &b);

    CHECK(a.status() == ConnectionStatus::Connected);
    CHECK(b.status() == ConnectionStatus::Connected);
    CHECK(a.localPlayerId() == 0);
    CHECK(b.localPlayerId() == 1);
    CHECK(a.hasReceivedState());

    TEST("a third client is rejected with a reason");
    {
        GameClient c;
        CHECK(c.beginConnect("127.0.0.1", PORT));
        pump(server, &c, nullptr);
        CHECK(c.status() == ConnectionStatus::Rejected);
        CHECK(c.rejectReason() == "Server full");
    }

    TEST("input from a stranger cannot drive a player");
    {
        // Spoof player 0 walking right, from an endpoint the server never
        // handed a slot to.
        UdpSocket attacker;
        CHECK(attacker.bind(0));
        float xBefore = server.sim().player(0).position().x;

        for (int i = 0; i < 60; ++i) {
            InputStatePacket pkt;
            pkt.playerId = 0;
            pkt.sequence = 1000 + i;
            pkt.moveX = 1.f;
            attacker.send(&pkt, sizeof(pkt), Endpoint{ "127.0.0.1", PORT });
            server.pumpNetwork();
            server.advance(TICK_DT);
        }
        CHECK_NEAR(server.sim().player(0).position().x, xBefore);
    }

    TEST("a stranger cannot disconnect a player");
    {
        UdpSocket attacker;
        CHECK(attacker.bind(0));
        DisconnectPacket pkt;
        pkt.playerId = 0;
        attacker.send(&pkt, sizeof(pkt), Endpoint{ "127.0.0.1", PORT });
        server.pumpNetwork();
        CHECK(server.sim().active(0));
    }

    TEST("a real client's input does move it");
    {
        float xBefore = server.sim().player(0).position().x;
        for (int i = 0; i < 30; ++i) {
            a.sendInput(1.f, 0.f, false, false, false, false);
            server.pumpNetwork();
            server.advance(TICK_DT);
        }
        CHECK(server.sim().player(0).position().x > xBefore);
    }

    TEST("stale snapshots are ignored");
    {
        uint32_t tickBefore = a.latestState().tick;
        pump(server, &a, &b, 2);
        CHECK(a.latestState().tick > tickBefore);
        // Replay an old packet: the client should keep the newer tick.
        StateUpdatePacket stale;
        stale.tick = 1;
        uint32_t current = a.latestState().tick;
        UdpSocket spoofer;
        spoofer.bind(0);
        spoofer.send(&stale, sizeof(stale), Endpoint{ "127.0.0.1", PORT });
        a.update(TICK_DT);
        CHECK(a.latestState().tick == current);
    }

    TEST("a silent client is timed out and its slot freed");
    {
        // Nobody sends anything for longer than CLIENT_TIMEOUT.
        int steps = static_cast<int>((CLIENT_TIMEOUT + 1.f) / TICK_DT);
        for (int i = 0; i < steps; ++i) server.advance(TICK_DT);
        CHECK(!server.sim().active(0));
        CHECK(!server.sim().active(1));
        CHECK(server.sim().activeCount() == 0);
    }

    TEST("a client can reconnect after disconnecting");
    {
        // This is the regression: close() used to leave the socket dead, so
        // going back to the title screen bricked the client for good.
        GameClient c;
        CHECK(c.beginConnect("127.0.0.1", PORT));
        pump(server, &c, nullptr);
        CHECK(c.status() == ConnectionStatus::Connected);

        c.disconnect();
        server.pumpNetwork();
        CHECK(c.status() == ConnectionStatus::Disconnected);

        CHECK(c.beginConnect("127.0.0.1", PORT));
        pump(server, &c, nullptr);
        CHECK(c.status() == ConnectionStatus::Connected);
        CHECK(c.hasReceivedState());
    }

    TEST("rematch restarts a finished match once both ask");
    {
        GameServer s2;
        if (!s2.start(PORT + 1)) return 1;
        GameClient p1, p2;
        p1.beginConnect("127.0.0.1", PORT + 1);
        p2.beginConnect("127.0.0.1", PORT + 1);
        pump(s2, &p1, &p2);
        CHECK(p1.status() == ConnectionStatus::Connected);
        CHECK(p2.status() == ConnectionStatus::Connected);

        // Walk them together and let p1 swing on a cycle until p2 goes down.
        const int swingPeriod = 80;   // long enough to get the stamina back
        for (int i = 0; i < 4000 && !s2.sim().matchOver(); ++i) {
            bool swing = (i % swingPeriod) == 0;
            p1.sendInput(1.f, 0.f, swing, false, false, false);
            p2.sendInput(-1.f, 0.f, false, false, false, false);
            s2.pumpNetwork();
            s2.advance(TICK_DT);
        }
        CHECK(s2.sim().matchOver());

        pump(s2, &p1, &p2);
        CHECK(p1.latestState().winnerId != NO_WINNER);

        p1.sendRematch();
        s2.pumpNetwork();
        CHECK(s2.sim().matchOver());   // still waiting on the other player

        p2.sendRematch();
        s2.pumpNetwork();
        CHECK(!s2.sim().matchOver());
        CHECK_NEAR(s2.sim().player(0).health(), MAX_HEALTH);
        CHECK_NEAR(s2.sim().player(1).health(), MAX_HEALTH);
    }

    return testSummary("network_test");
}
