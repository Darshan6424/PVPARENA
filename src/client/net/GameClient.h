#pragma once
// Talks to the server and hangs onto the newest snapshot it sent. No gameplay
// logic - rendering reads latestState() and nothing else.
//
// TODO: snapshots are drawn as they arrive, so jitter on the wire shows up as
// stutter. Buffering ~100ms and interpolating between the last two would smooth
// it out without giving the client any authority.

#include "common/Protocol.h"
#include "common/UdpSocket.h"
#include <string>

enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Rejected,
    TimedOut,
};

class GameClient {
public:
    // Non-blocking; call update() every frame to drive the handshake.
    // False means we couldn't even open a socket.
    bool beginConnect(const std::string& serverIp, uint16_t port);

    void update(float dt);

    void sendInput(float moveX, float moveY, bool attack, bool block, bool parry, bool dodge);
    void sendRematch();
    void disconnect();

    ConnectionStatus status() const { return status_; }
    uint8_t localPlayerId() const { return playerId_; }
    const net::StateUpdatePacket& latestState() const { return latestState_; }
    bool hasReceivedState() const { return hasReceivedState_; }
    const std::string& rejectReason() const { return rejectReason_; }

private:
    void handlePacket(const char* buffer, int size);

    UdpSocket sock_;
    Endpoint  serverEndpoint_;
    uint8_t   playerId_ = net::NO_WINNER;
    uint32_t  inputSequence_ = 0;
    ConnectionStatus status_ = ConnectionStatus::Disconnected;
    std::string rejectReason_;

    float connectRetryTimer_ = 0.f;
    float connectElapsed_ = 0.f;

    net::StateUpdatePacket latestState_{};
    bool hasReceivedState_ = false;
};
