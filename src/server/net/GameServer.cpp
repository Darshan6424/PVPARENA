#include "server/net/GameServer.h"
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>

using namespace net;

namespace {
// After a stall (the host dragging a window, a laptop waking up) drop the
// backlog rather than replaying the whole gap in one go.
constexpr float MAX_CATCHUP = 0.25f;
}

void Session::open(const Endpoint& from) {
    *this = Session{};
    open_ = true;
    endpoint_ = from;
}

void Session::close() {
    *this = Session{};
}

bool Session::acceptSequence(uint32_t sequence) {
    if (sequence != 0 && sequence < lastSequence_) return false;
    lastSequence_ = sequence;
    return true;
}

bool GameServer::start(uint16_t port) {
    if (!sock_.bind(port)) {
        std::fprintf(stderr, "GameServer: could not bind UDP port %u\n", port);
        return false;
    }
    sessions_ = {};
    sim_ = ArenaSim{};
    tickCounter_ = 0;
    std::printf("GameServer: listening on UDP port %u\n", port);
    return true;
}

void GameServer::stop() {
    sock_.close();
}

void GameServer::run(std::atomic<bool>& keepRunning) {
    using clock = std::chrono::steady_clock;
    auto prev = clock::now();
    float accumulator = 0.f;

    while (keepRunning.load()) {
        pumpNetwork();

        auto now = clock::now();
        accumulator += std::chrono::duration<float>(now - prev).count();
        prev = now;
        if (accumulator > MAX_CATCHUP) accumulator = MAX_CATCHUP;

        bool ticked = false;
        while (accumulator >= TICK_DT) {
            advance(TICK_DT);
            accumulator -= TICK_DT;
            ticked = true;
        }
        if (ticked) broadcastState();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int GameServer::slotFor(const Endpoint& from) const {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (sessions_[i].belongsTo(from)) return i;
    }
    return -1;
}

void GameServer::pumpNetwork() {
    char buffer[512];
    Endpoint from;

    while (true) {
        int received = sock_.receive(buffer, sizeof(buffer), from);
        if (received <= 0) break;
        if (received < static_cast<int>(sizeof(PacketHeader))) continue;

        PacketHeader header;
        std::memcpy(&header, buffer, sizeof(header));
        if (header.magic != PROTOCOL_MAGIC) continue;
        if (header.version != PROTOCOL_VERSION) continue;

        int slot = slotFor(from);
        if (slot >= 0) sessions_[slot].heardFromClient();

        switch (header.type) {
        case PacketType::ConnectRequest:
            onConnectRequest(from);
            break;

        case PacketType::InputState: {
            if (slot < 0) break;
            if (received != static_cast<int>(sizeof(InputStatePacket))) break;
            InputStatePacket pkt;
            std::memcpy(&pkt, buffer, sizeof(pkt));
            onInput(slot, pkt);
            break;
        }

        case PacketType::Disconnect:
            if (slot >= 0) dropPlayer(slot, "left");
            break;

        case PacketType::Rematch:
            if (slot >= 0) {
                sessions_[slot].requestRematch();
                startRematchIfEveryoneAgrees();
            }
            break;

        default:
            break;
        }
    }
}

void GameServer::onConnectRequest(const Endpoint& from) {
    int existing = slotFor(from);
    if (existing >= 0) {
        // Their accept probably got lost. Send another one.
        ConnectAcceptPacket accept;
        accept.assignedPlayerId = static_cast<uint8_t>(existing);
        sock_.send(&accept, sizeof(accept), from);
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (sessions_[i].isOpen()) continue;

        sessions_[i].open(from);

        // Give whoever arrives after the last match a fresh one, otherwise a
        // dedicated server stays stuck on a win screen forever.
        if (sim_.matchOver()) sim_.resetMatch();
        sim_.addPlayer(i);

        ConnectAcceptPacket accept;
        accept.assignedPlayerId = static_cast<uint8_t>(i);
        sock_.send(&accept, sizeof(accept), from);

        std::printf("GameServer: player %d connected from %s:%u\n",
                    i, from.ip.c_str(), from.port);
        return;
    }

    ConnectRejectPacket reject;
    std::snprintf(reject.reason, sizeof(reject.reason), "Server full");
    sock_.send(&reject, sizeof(reject), from);
}

void GameServer::onInput(int slot, const InputStatePacket& pkt) {
    if (!sessions_[slot].acceptSequence(pkt.sequence)) return;

    PlayerInput in;
    in.moveX = pkt.moveX;
    in.moveY = pkt.moveY;
    in.attack = pkt.attack != 0;
    in.block = pkt.block != 0;
    in.parry = pkt.parry != 0;
    in.dodge = pkt.dodge != 0;
    sim_.setInput(slot, in);
}

void GameServer::dropPlayer(int slot, const char* why) {
    if (!sessions_[slot].isOpen()) return;
    std::printf("GameServer: player %d %s\n", slot, why);

    sessions_[slot].close();
    sim_.removePlayer(slot);

    // Don't leave whoever is still here staring at a finished match.
    if (sim_.matchOver()) sim_.resetMatch();
}

void GameServer::startRematchIfEveryoneAgrees() {
    if (!sim_.matchOver()) return;

    int connected = 0;
    for (const Session& s : sessions_) {
        if (!s.isOpen()) continue;
        ++connected;
        if (!s.wantsRematch()) return;
    }
    if (connected == 0) return;

    sim_.resetMatch();
    for (Session& s : sessions_) {
        s.clearRematch();
    }
}

void GameServer::advance(float dt) {
    ++tickCounter_;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!sessions_[i].isOpen()) continue;
        sessions_[i].addSilence(dt);
        if (sessions_[i].hasGoneQuiet()) {
            dropPlayer(i, "timed out");
        }
    }

    sim_.step(dt);
}

void GameServer::broadcastState() {
    StateUpdatePacket update;
    update.tick = tickCounter_;
    update.winnerId = sim_.winner();
    sim_.fillSnapshots(update.players);

    for (const Session& s : sessions_) {
        if (s.isOpen()) {
            sock_.send(&update, sizeof(update), s.endpoint());
        }
    }
}
