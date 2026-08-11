#pragma once
// Owns the socket and the player sessions, and drives ArenaSim at a fixed
// tick. All the game rules live in ArenaSim and Fighter; this is plumbing.

#include "server/sim/ArenaSim.h"
#include "common/Protocol.h"
#include "common/UdpSocket.h"
#include <atomic>
#include <array>

// One connected client. Knows the address it belongs to and keeps the small
// amount of per-connection bookkeeping that goes with it.
class Session {
public:
    void open(const Endpoint& from);
    void close();
    bool isOpen() const { return open_; }

    const Endpoint& endpoint() const { return endpoint_; }
    bool belongsTo(const Endpoint& from) const { return open_ && endpoint_ == from; }

    // False if this packet is older than one we already applied. UDP reorders.
    bool acceptSequence(uint32_t sequence);

    void heardFromClient() { silentFor_ = 0.f; }
    void addSilence(float dt) { silentFor_ += dt; }
    bool hasGoneQuiet() const { return silentFor_ >= net::CLIENT_TIMEOUT; }

    void requestRematch() { wantsRematch_ = true; }
    bool wantsRematch() const { return wantsRematch_; }
    void clearRematch() { wantsRematch_ = false; }

private:
    bool open_ = false;
    Endpoint endpoint_;
    uint32_t lastSequence_ = 0;
    float silentFor_ = 0.f;
    bool wantsRematch_ = false;
};

class GameServer {
public:
    bool start(uint16_t port);
    void stop();

    // Blocks until keepRunning goes false. Meant to run on its own thread.
    void run(std::atomic<bool>& keepRunning);

    // Exposed so tests can drive the loop themselves instead of waiting.
    void pumpNetwork();
    void advance(float dt);
    void broadcastState();
    const ArenaSim& sim() const { return sim_; }

private:
    // Sessions are found by the address a packet came from, never by the id
    // written inside it. Otherwise anyone could drive either player.
    int slotFor(const Endpoint& from) const;

    void onConnectRequest(const Endpoint& from);
    void onInput(int slot, const net::InputStatePacket& pkt);
    void dropPlayer(int slot, const char* why);
    void startRematchIfEveryoneAgrees();

    UdpSocket sock_;
    ArenaSim sim_;
    std::array<Session, net::MAX_PLAYERS> sessions_;
    uint32_t tickCounter_ = 0;
};
