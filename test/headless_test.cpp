// headless_test.cpp - not part of the shipped game.
// Spins up the real GameServer, connects two fake UDP "clients", makes
// player 0 walk into range and attack, and prints the resulting state so
// we can sanity-check damage/stamina/parry math without needing SFML.

#include "../server/GameServer.h"
#include "../common/Protocol.h"
#include "../common/UdpSocket.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>

using namespace net;

int main() {
    GameServer server;
    if (!server.start(9500)) return 1;

    std::atomic<bool> running{ true };
    std::thread serverThread([&]() { server.run(running); });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    UdpSocket clientA, clientB;
    clientA.bind(0);
    clientB.bind(0);
    Endpoint serverEp{ "127.0.0.1", 9500 };

    auto connect = [&](UdpSocket& s) -> uint8_t {
        ConnectRequestPacket req;
        s.send(&req, sizeof(req), serverEp);
        char buf[512];
        Endpoint from;
        for (int i = 0; i < 50; ++i) {
            int r = s.receive(buf, sizeof(buf), from);
            if (r == sizeof(ConnectAcceptPacket)) {
                ConnectAcceptPacket acc;
                std::memcpy(&acc, buf, sizeof(acc));
                return acc.assignedPlayerId;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return 255;
    };

    uint8_t idA = connect(clientA);
    uint8_t idB = connect(clientB);
    std::printf("Client A got id %d, Client B got id %d\n", idA, idB);

    auto sendInput = [&](UdpSocket& s, uint8_t id, uint32_t seq, float mx, float my,
                          uint8_t attack, uint8_t block, uint8_t parry, uint8_t dodge) {
        InputStatePacket in;
        in.playerId = id;
        in.sequence = seq;
        in.moveX = mx; in.moveY = my;
        in.attack = attack; in.block = block; in.parry = parry; in.dodge = dodge;
        s.send(&in, sizeof(in), serverEp);
    };

    StateUpdatePacket lastState{};
    auto drain = [&](UdpSocket& s) {
        char buf[512];
        Endpoint from;
        int r;
        while ((r = s.receive(buf, sizeof(buf), from)) > 0) {
            if (r == sizeof(StateUpdatePacket)) {
                std::memcpy(&lastState, buf, sizeof(lastState));
            }
        }
    };

    // Walk player A toward player B until they're in melee range
    // (660px gap / 190px/s ~= 3.5s of walking).
    for (int i = 0; i < 130; ++i) {
        sendInput(clientA, idA, i + 1, 1.f, 0.f, 0, 0, 0, 0);
        sendInput(clientB, idB, i + 1, 0.f, 0.f, 0, 0, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    drain(clientA);
    std::printf("After walking: A.x=%.1f B.x=%.1f dist=%.1f\n",
                lastState.players[idA].x, lastState.players[idB].x,
                std::fabs(lastState.players[idA].x - lastState.players[idB].x));

    // A attacks. B does nothing (should take full damage).
    sendInput(clientA, idA, 150, 0.f, 0.f, 1, 0, 0, 0);
    sendInput(clientB, idB, 150, 0.f, 0.f, 0, 0, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    drain(clientA);
    std::printf("After attack #1: B health=%.1f B state=%d A stamina=%.1f\n",
                lastState.players[idB].health, (int)lastState.players[idB].state,
                lastState.players[idA].stamina);

    // Let stagger/recovery clear.
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    for (int i = 0; i < 5; ++i) {
        sendInput(clientA, idA, 200 + i, 0.f, 0.f, 0, 0, 0, 0);
        sendInput(clientB, idB, 200 + i, 0.f, 0.f, 0, 0, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    // B parries, A attacks into it - A should get staggered, B takes no damage.
    sendInput(clientB, idB, 300, 0.f, 0.f, 0, 0, 1, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(60)); // let parry become live
    sendInput(clientA, idA, 300, 0.f, 0.f, 1, 0, 0, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    drain(clientA);
    std::printf("After parry: B health=%.1f A state=%d (6=Staggered)\n",
                lastState.players[idB].health, (int)lastState.players[idA].state);

    running = false;
    serverThread.join();
    server.stop();
    std::printf("Test complete.\n");
    return 0;
}
