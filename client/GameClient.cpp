#include "GameClient.h"
#include <cstring>
#include <cstdio>

using namespace net;

namespace {
    constexpr float CONNECT_RETRY_INTERVAL = 0.5f;
    constexpr float CONNECT_TIMEOUT = 5.f;
}

bool GameClient::beginConnect(const std::string& serverIp, uint16_t port) {
    if (!sock_.bind(0)) { // ephemeral local port
        return false;
    }
    serverEndpoint_ = { serverIp, port };
    status_ = ConnectionStatus::Connecting;
    connectRetryTimer_ = 0.f;
    connectTimeoutTotal_ = 0.f;
    hasReceivedState_ = false;

    ConnectRequestPacket req;
    sock_.send(&req, sizeof(req), serverEndpoint_);
    return true;
}

void GameClient::update(float dt) {
    if (status_ == ConnectionStatus::Connecting) {
        connectRetryTimer_ += dt;
        connectTimeoutTotal_ += dt;

        if (connectTimeoutTotal_ >= CONNECT_TIMEOUT) {
            status_ = ConnectionStatus::TimedOut;
            return;
        }
        if (connectRetryTimer_ >= CONNECT_RETRY_INTERVAL) {
            connectRetryTimer_ = 0.f;
            ConnectRequestPacket req;
            sock_.send(&req, sizeof(req), serverEndpoint_);
        }
    }

    if (status_ == ConnectionStatus::Disconnected ||
        status_ == ConnectionStatus::Rejected ||
        status_ == ConnectionStatus::TimedOut) {
        return;
    }

    char buffer[512];
    Endpoint from;
    while (true) {
        int received = sock_.receive(buffer, sizeof(buffer), from);
        if (received <= 0) break;
        if (received < static_cast<int>(sizeof(PacketHeader))) continue;

        PacketHeader header;
        std::memcpy(&header, buffer, sizeof(header));
        if (header.magic != PROTOCOL_MAGIC) continue;

        switch (header.type) {
            case PacketType::ConnectAccept: {
                if (received != static_cast<int>(sizeof(ConnectAcceptPacket))) break;
                ConnectAcceptPacket pkt;
                std::memcpy(&pkt, buffer, sizeof(pkt));
                playerId_ = pkt.assignedPlayerId;
                status_ = ConnectionStatus::Connected;
                break;
            }
            case PacketType::ConnectReject: {
                status_ = ConnectionStatus::Rejected;
                break;
            }
            case PacketType::StateUpdate: {
                if (received != static_cast<int>(sizeof(StateUpdatePacket))) break;
                std::memcpy(&latestState_, buffer, sizeof(latestState_));
                hasReceivedState_ = true;
                break;
            }
            default:
                break;
        }
    }
}

void GameClient::sendInput(float moveX, float moveY, bool attack, bool block, bool parry, bool dodge) {
    if (status_ != ConnectionStatus::Connected) return;

    InputStatePacket pkt;
    pkt.playerId = playerId_;
    pkt.sequence = ++inputSequence_;
    pkt.moveX = moveX;
    pkt.moveY = moveY;
    pkt.attack = attack ? 1 : 0;
    pkt.block  = block  ? 1 : 0;
    pkt.parry  = parry  ? 1 : 0;
    pkt.dodge  = dodge  ? 1 : 0;

    sock_.send(&pkt, sizeof(pkt), serverEndpoint_);
}

void GameClient::disconnect() {
    if (status_ == ConnectionStatus::Connected) {
        DisconnectPacket pkt;
        pkt.playerId = playerId_;
        sock_.send(&pkt, sizeof(pkt), serverEndpoint_);
    }
    sock_.close();
    status_ = ConnectionStatus::Disconnected;
    hasReceivedState_ = false;
}
