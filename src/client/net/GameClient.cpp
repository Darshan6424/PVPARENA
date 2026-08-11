#include "client/net/GameClient.h"
#include <cstring>

using namespace net;

namespace {
constexpr float CONNECT_RETRY_INTERVAL = 0.5f;
constexpr float CONNECT_TIMEOUT = 5.f;
}

bool GameClient::beginConnect(const std::string& serverIp, uint16_t port) {
    sock_.close();                 // start from a clean socket every attempt
    if (!sock_.bind(0)) {
        status_ = ConnectionStatus::Disconnected;
        return false;
    }

    serverEndpoint_ = { serverIp, port };
    playerId_ = NO_WINNER;
    inputSequence_ = 0;
    status_ = ConnectionStatus::Connecting;
    connectRetryTimer_ = 0.f;
    connectElapsed_ = 0.f;
    hasReceivedState_ = false;
    latestState_ = StateUpdatePacket{};
    rejectReason_.clear();

    ConnectRequestPacket req;
    sock_.send(&req, sizeof(req), serverEndpoint_);
    return true;
}

void GameClient::update(float dt) {
    if (status_ != ConnectionStatus::Connecting && status_ != ConnectionStatus::Connected) {
        return;
    }

    if (status_ == ConnectionStatus::Connecting) {
        connectElapsed_ += dt;
        connectRetryTimer_ += dt;

        if (connectElapsed_ >= CONNECT_TIMEOUT) {
            status_ = ConnectionStatus::TimedOut;
            sock_.close();
            return;
        }
        if (connectRetryTimer_ >= CONNECT_RETRY_INTERVAL) {
            connectRetryTimer_ = 0.f;
            ConnectRequestPacket req;
            sock_.send(&req, sizeof(req), serverEndpoint_);
        }
    }

    char buffer[512];
    Endpoint from;
    while (true) {
        int received = sock_.receive(buffer, sizeof(buffer), from);
        if (received <= 0) break;
        if (from != serverEndpoint_) continue;   // not from our server
        handlePacket(buffer, received);
    }
}

void GameClient::handlePacket(const char* buffer, int size) {
    if (size < static_cast<int>(sizeof(PacketHeader))) return;

    PacketHeader header;
    std::memcpy(&header, buffer, sizeof(header));
    if (header.magic != PROTOCOL_MAGIC) return;
    if (header.version != PROTOCOL_VERSION) return;

    switch (header.type) {
    case PacketType::ConnectAccept: {
        if (size != static_cast<int>(sizeof(ConnectAcceptPacket))) return;
        ConnectAcceptPacket pkt;
        std::memcpy(&pkt, buffer, sizeof(pkt));
        if (pkt.assignedPlayerId >= MAX_PLAYERS) return;
        playerId_ = pkt.assignedPlayerId;
        status_ = ConnectionStatus::Connected;
        break;
    }
    case PacketType::ConnectReject: {
        if (size == static_cast<int>(sizeof(ConnectRejectPacket))) {
            ConnectRejectPacket pkt;
            std::memcpy(&pkt, buffer, sizeof(pkt));
            pkt.reason[sizeof(pkt.reason) - 1] = '\0';
            rejectReason_ = pkt.reason;
        }
        status_ = ConnectionStatus::Rejected;
        sock_.close();
        break;
    }
    case PacketType::StateUpdate: {
        if (size != static_cast<int>(sizeof(StateUpdatePacket))) return;
        StateUpdatePacket pkt;
        std::memcpy(&pkt, buffer, sizeof(pkt));
        // UDP reorders; a stale snapshot would yank everyone backwards.
        if (hasReceivedState_ && pkt.tick <= latestState_.tick) return;
        latestState_ = pkt;
        hasReceivedState_ = true;
        break;
    }
    default:
        break;
    }
}

void GameClient::sendInput(float moveX, float moveY, bool attack, bool block,
                           bool parry, bool dodge) {
    if (status_ != ConnectionStatus::Connected) return;

    InputStatePacket pkt;
    pkt.playerId = playerId_;
    pkt.sequence = ++inputSequence_;
    pkt.moveX = moveX;
    pkt.moveY = moveY;
    pkt.attack = attack ? 1 : 0;
    pkt.block = block ? 1 : 0;
    pkt.parry = parry ? 1 : 0;
    pkt.dodge = dodge ? 1 : 0;

    sock_.send(&pkt, sizeof(pkt), serverEndpoint_);
}

void GameClient::sendRematch() {
    if (status_ != ConnectionStatus::Connected) return;
    RematchPacket pkt;
    pkt.playerId = playerId_;
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
    playerId_ = NO_WINNER;
}
