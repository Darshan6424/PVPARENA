#pragma once
// Runs a GameServer on a background thread inside the client process, for the
// "Create Server" and "Local Co-op" buttons. Joins the thread on destruction
// so there's no way to leave it running by accident.

#include "server/net/GameServer.h"
#include <atomic>
#include <memory>
#include <thread>

class EmbeddedServer {
public:
    ~EmbeddedServer() { stop(); }

    bool start(uint16_t port) {
        stop();
        auto server = std::make_unique<GameServer>();
        if (!server->start(port)) return false;

        server_ = std::move(server);
        keepRunning_ = true;
        thread_ = std::thread([this]() { server_->run(keepRunning_); });
        return true;
    }

    void stop() {
        keepRunning_ = false;
        if (thread_.joinable()) thread_.join();
        if (server_) {
            server_->stop();
            server_.reset();
        }
    }

    bool running() const { return server_ != nullptr; }

private:
    std::unique_ptr<GameServer> server_;
    std::thread thread_;
    std::atomic<bool> keepRunning_{ false };
};
