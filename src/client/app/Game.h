#pragma once
// Owns the window and everything with a lifetime as long as the program:
// assets, renderer, audio, the network clients and the embedded server.
//
// It does not draw anything itself. Whatever Screen is active does that.
// Game's job is to hold the shared pieces and to switch between screens.

#include "client/view/Assets.h"
#include "client/view/AudioManager.h"
#include "client/net/EmbeddedServer.h"
#include "client/net/GameClient.h"
#include "client/input/InputCapture.h"
#include "client/app/MatchWatcher.h"
#include "client/view/Renderer.h"
#include "client/screens/Screen.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <random>
#include <string>

class Game {
public:
    Game();

    int run();

    // Takes effect at the end of the current frame, not immediately. A screen
    // usually asks for this from inside its own handleEvent, and destroying it
    // mid-call would pull the ground out from under the caller.
    void changeScreen(std::unique_ptr<Screen> next);

    // Screen transitions.
    void showTitle();
    void showAddressEntry();
    void startHosting(bool localCoop);
    void startJoining();
    void leaveMatch();

    sf::RenderWindow& window() { return window_; }
    Renderer& renderer() { return renderer_; }
    AudioManager& audio() { return audio_; }
    Assets& assets() { return assets_; }

    GameClient& client() { return client_; }
    GameClient& secondClient() { return clientP2_; }
    InputCapture& player1Input() { return input1_; }
    InputCapture& player2Input() { return input2_; }
    MatchWatcher& watcher() { return watcher_; }

    bool isLocalCoop() const { return localCoop_; }
    const std::string& statusMessage() const { return status_; }
    void setStatusMessage(std::string text) { status_ = std::move(text); }
    const std::string& hostAddress() const { return hostAddress_; }
    std::string& addressText() { return addressText_; }

    void resetMatchState();
    void shakeScreen();
    void applyScreenShake(float dt);

    sf::Vector2f mousePosition() const { return mouse_; }
    bool hovering(const sf::FloatRect& rect) const { return rect.contains(mouse_); }

private:
    sf::RenderWindow window_;
    Assets assets_;
    Renderer renderer_;
    AudioManager audio_;

    GameClient client_;
    GameClient clientP2_;          // local co-op only
    InputCapture input1_{ PLAYER1_KEYS };
    InputCapture input2_{ PLAYER2_KEYS };
    MatchWatcher watcher_;
    EmbeddedServer embedded_;

    std::unique_ptr<Screen> screen_;
    std::unique_ptr<Screen> nextScreen_;

    bool localCoop_ = false;
    std::string addressText_ = "127.0.0.1";
    std::string status_;
    std::string hostAddress_;

    float shake_ = 0.f;
    sf::Vector2f mouse_;
    std::mt19937 rng_{ std::random_device{}() };
};
