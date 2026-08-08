// main_server.cpp
// Standalone dedicated server - run this on a machine both players can
// reach, then have both clients "Join" its IP address.

#include "GameServer.h"
#include "../common/Protocol.h"
#include <atomic>
#include <csignal>
#include <cstdio>

namespace {
    std::atomic<bool> g_running{ true };
}

void handleSigint(int) {
    g_running = false;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSigint);

    uint16_t port = net::DEFAULT_SERVER_PORT;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    GameServer server;
    if (!server.start(port)) {
        std::fprintf(stderr, "Failed to start server on port %u\n", port);
        return 1;
    }

    std::printf("Server running on port %u. Press Ctrl+C to stop.\n", port);
    server.run(g_running);

    std::printf("Server shutting down.\n");
    server.stop();
    return 0;
}
