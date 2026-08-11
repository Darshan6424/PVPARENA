// Dedicated server. Run it on a machine both players can reach, then have
// them Join its address. Optional port argument, default 9422.

#include "server/net/GameServer.h"
#include "common/Protocol.h"
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>

namespace {
std::atomic<bool> g_running{ true };

void onStopSignal(int) {
    g_running = false;
}
}

int main(int argc, char** argv) {
    // Under systemd stdout is a pipe, which makes printf block-buffered - logs
    // would sit in the buffer instead of reaching journalctl.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    std::signal(SIGINT, onStopSignal);
    std::signal(SIGTERM, onStopSignal);   // what systemctl stop sends

    uint16_t port = net::DEFAULT_SERVER_PORT;
    if (argc > 1) {
        int parsed = std::atoi(argv[1]);
        if (parsed <= 0 || parsed > 65535) {
            std::fprintf(stderr, "Bad port '%s'\n", argv[1]);
            return 1;
        }
        port = static_cast<uint16_t>(parsed);
    }

    GameServer server;
    if (!server.start(port)) return 1;

    std::printf("Listening on port %u. Ctrl+C to stop.\n", port);
    server.run(g_running);
    server.stop();
    std::printf("Stopped.\n");
    return 0;
}
