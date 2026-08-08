#pragma once
// GameClient.h
// Everything the client needs to talk to the server. This class holds
// NO gameplay logic of its own - it just ships input packets out and
// keeps the most recent StateUpdate the server sent back. Rendering
// reads from `latestState()` and nothing else.

#include "../common/Protocol.h"
#include "../common/UdpSocket.h"
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
    // Kicks off a connect attempt to serverIp:port. Non-blocking - call
    // update() every frame afterward to drive the handshake and pump
    // incoming state.
    bool beginConnect(const std::string& serverIp, uint16_t port);

    // Call once per client frame. Handles the handshake retries and
    // drains any pending StateUpdate packets.
    void update(float dt);

    void sendInput(float moveX, float moveY, bool attack, bool block, bool parry, bool dodge);

    ConnectionStatus status() const { return status_; }
    uint8_t          localPlayerId() const { return playerId_; }
    const net::StateUpdatePacket& latestState() const { return latestState_; }
    bool hasReceivedState() const { return hasReceivedState_; }

    void disconnect();

private:
    UdpSocket   sock_;
    Endpoint    serverEndpoint_;
    uint8_t     playerId_ = 255;
    uint32_t    inputSequence_ = 0;
    ConnectionStatus status_ = ConnectionStatus::Disconnected;

    float connectRetryTimer_ = 0.f;
    float connectTimeoutTotal_ = 0.f;

    net::StateUpdatePacket latestState_{};
    bool hasReceivedState_ = false;
};
