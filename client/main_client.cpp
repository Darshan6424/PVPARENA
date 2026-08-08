// main_client.cpp
// Title screen -> Create Server (embedded, runs on its own thread inside
// this same process) or Join (type an IP) -> gameplay -> win/lose screen.
//
// Controls:
//   WASD        move
//   J / LClick  attack
//   K / RClick  parry (short window - time it against the opponent's swing)
//   Space       dodge
//   Left Shift  hold to block

#include "GameClient.h"
#include "Renderer.h"
#include "../server/GameServer.h"
#include "../common/Protocol.h"
#include <SFML/Graphics.hpp>
#include <optional>
#include <atomic>
#include <thread>
#include <memory>
#include <cctype>

using namespace net;

enum class AppState {
    Title,
    EnteringIp,
    Connecting,
    Playing,
    GameOver,
};

namespace {

bool buttonContains(const sf::FloatRect& rect, sf::Vector2f point) {
    return rect.contains(point);
}

void drawButton(sf::RenderWindow& window, const sf::Font& font, bool fontLoaded,
                 const sf::FloatRect& rect, const std::string& label, bool hovered) {
    sf::RectangleShape box({ rect.size.x, rect.size.y });
    box.setPosition({ rect.position.x, rect.position.y });
    box.setFillColor(hovered ? sf::Color(90, 100, 120) : sf::Color(60, 65, 78));
    box.setOutlineColor(sf::Color::White);
    box.setOutlineThickness(2.f);
    window.draw(box);

    if (fontLoaded) {
        sf::Text text(font, label, 20);
        sf::FloatRect bounds = text.getLocalBounds();
        text.setOrigin({ bounds.position.x + bounds.size.x / 2.f,
                          bounds.position.y + bounds.size.y / 2.f });
        text.setPosition({ rect.position.x + rect.size.x / 2.f,
                            rect.position.y + rect.size.y / 2.f });
        text.setFillColor(sf::Color::White);
        window.draw(text);
    }
}

} // namespace

int main() {
    sf::RenderWindow window(sf::VideoMode({ static_cast<unsigned>(ARENA_WIDTH),
                                             static_cast<unsigned>(ARENA_HEIGHT) }),
                             "PvP Arena");
    window.setFramerateLimit(60);

    Renderer renderer;

    GameClient client;
    AppState appState = AppState::Title;

    std::string ipInput = "127.0.0.1";
    std::string statusMessage;

    // Embedded server support ("Create Server" button).
    std::unique_ptr<GameServer> embeddedServer;
    std::unique_ptr<std::thread> serverThread;
    std::atomic<bool> serverRunning{ false };

    auto shutdownEmbeddedServer = [&]() {
        if (serverThread) {
            serverRunning = false;
            serverThread->join();
            serverThread.reset();
        }
        if (embeddedServer) {
            embeddedServer->stop();
            embeddedServer.reset();
        }
    };

    sf::FloatRect createBtn({ ARENA_WIDTH / 2.f - 110.f, 220.f }, { 220.f, 50.f });
    sf::FloatRect joinBtn({ ARENA_WIDTH / 2.f - 110.f, 290.f }, { 220.f, 50.f });
    sf::FloatRect connectBtn({ ARENA_WIDTH / 2.f - 90.f, 300.f }, { 180.f, 44.f });
    sf::FloatRect backBtn({ 20.f, 20.f }, { 90.f, 36.f });

    bool prevAttackKey = false, prevParryKey = false, prevDodgeKey = false;

    sf::Clock clock;
    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (appState == AppState::Title) {
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        if (buttonContains(createBtn, mousePos)) {
                            embeddedServer = std::make_unique<GameServer>();
                            if (embeddedServer->start(DEFAULT_SERVER_PORT)) {
                                serverRunning = true;
                                serverThread = std::make_unique<std::thread>(
                                    [&]() { embeddedServer->run(serverRunning); });
                                client.beginConnect("127.0.0.1", DEFAULT_SERVER_PORT);
                                appState = AppState::Connecting;
                                statusMessage = "Starting server and connecting...";
                            } else {
                                statusMessage = "Failed to start server (port in use?)";
                            }
                        } else if (buttonContains(joinBtn, mousePos)) {
                            appState = AppState::EnteringIp;
                        }
                    }
                }
            } else if (appState == AppState::EnteringIp) {
                if (const auto* te = event->getIf<sf::Event::TextEntered>()) {
                    if (te->unicode == 8) { // backspace
                        if (!ipInput.empty()) ipInput.pop_back();
                    } else if (te->unicode == 13) { // enter
                        client.beginConnect(ipInput, DEFAULT_SERVER_PORT);
                        appState = AppState::Connecting;
                        statusMessage = "Connecting to " + ipInput + " ...";
                    } else if (te->unicode < 128) {
                        char c = static_cast<char>(te->unicode);
                        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                            if (ipInput.size() < 45) ipInput += c;
                        }
                    }
                }
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        if (buttonContains(connectBtn, mousePos)) {
                            client.beginConnect(ipInput, DEFAULT_SERVER_PORT);
                            appState = AppState::Connecting;
                            statusMessage = "Connecting to " + ipInput + " ...";
                        } else if (buttonContains(backBtn, mousePos)) {
                            appState = AppState::Title;
                        }
                    }
                }
            }

            if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                if (kp->code == sf::Keyboard::Key::Escape) {
                    if (appState == AppState::Playing || appState == AppState::GameOver) {
                        client.disconnect();
                        shutdownEmbeddedServer();
                        appState = AppState::Title;
                    } else if (appState == AppState::EnteringIp) {
                        appState = AppState::Title;
                    }
                }
            }
        }

        // ---- Connecting: drive the handshake until it resolves --------
        if (appState == AppState::Connecting) {
            client.update(dt);
            if (client.status() == ConnectionStatus::Connected) {
                appState = AppState::Playing;
            } else if (client.status() == ConnectionStatus::Rejected) {
                statusMessage = "Server rejected connection (full?)";
                shutdownEmbeddedServer();
                appState = AppState::Title;
            } else if (client.status() == ConnectionStatus::TimedOut) {
                statusMessage = "Connection timed out";
                shutdownEmbeddedServer();
                appState = AppState::Title;
            }
        }

        // ---- Playing: capture input, send it, pump state --------------
        if (appState == AppState::Playing || appState == AppState::GameOver) {
            client.update(dt);

            float moveX = 0.f, moveY = 0.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) moveX -= 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) moveX += 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) moveY -= 1.f;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) moveY += 1.f;

            bool attackHeld = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::J) ||
                               sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
            bool parryHeld  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::K) ||
                               sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
            bool dodgeHeld  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
            bool blockHeld  = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);

            bool attackPressed = attackHeld && !prevAttackKey;
            bool parryPressed  = parryHeld  && !prevParryKey;
            bool dodgePressed  = dodgeHeld  && !prevDodgeKey;
            prevAttackKey = attackHeld;
            prevParryKey  = parryHeld;
            prevDodgeKey  = dodgeHeld;

            if (appState == AppState::Playing) {
                client.sendInput(moveX, moveY, attackPressed, blockHeld, parryPressed, dodgePressed);
            }

            if (client.hasReceivedState() && client.latestState().winnerId != 255) {
                appState = AppState::GameOver;
            }
        }

        // ---------------------------------------------------------------
        // Draw
        // ---------------------------------------------------------------
        window.clear(sf::Color(20, 22, 28));

        if (appState == AppState::Title) {
            renderer.drawCenteredText(window, "PVP ARENA", ARENA_WIDTH / 2.f, 120.f, 36,
                                       sf::Color::White);
            bool hoverCreate = buttonContains(createBtn, mousePos);
            bool hoverJoin = buttonContains(joinBtn, mousePos);
            drawButton(window, renderer.font(), renderer.fontLoaded(), createBtn, "Create Server", hoverCreate);
            drawButton(window, renderer.font(), renderer.fontLoaded(), joinBtn, "Join", hoverJoin);
            if (!statusMessage.empty()) {
                renderer.drawCenteredText(window, statusMessage, ARENA_WIDTH / 2.f, 380.f, 14,
                                           sf::Color(255, 150, 150));
            }
        } else if (appState == AppState::EnteringIp) {
            renderer.drawCenteredText(window, "Enter server IP", ARENA_WIDTH / 2.f, 200.f, 24,
                                       sf::Color::White);
            renderer.drawCenteredText(window, ipInput, ARENA_WIDTH / 2.f, 250.f, 28,
                                       sf::Color(120, 220, 255));
            drawButton(window, renderer.font(), renderer.fontLoaded(), connectBtn, "Connect",
                       buttonContains(connectBtn, mousePos));
            drawButton(window, renderer.font(), renderer.fontLoaded(), backBtn, "Back",
                       buttonContains(backBtn, mousePos));
        } else if (appState == AppState::Connecting) {
            renderer.drawCenteredText(window, statusMessage, ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f,
                                       20, sf::Color::White);
        } else { // Playing or GameOver
            renderer.drawArena(window);
            if (client.hasReceivedState()) {
                const StateUpdatePacket& state = client.latestState();
                for (int i = 0; i < MAX_PLAYERS; ++i) {
                    renderer.drawPlayer(window, state.players[i], static_cast<uint8_t>(i),
                                         client.localPlayerId());
                }
                renderer.drawHud(window, state, client.localPlayerId());
            } else {
                renderer.drawCenteredText(window, "Waiting for opponent...",
                                           ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f, 20,
                                           sf::Color::White);
            }
        }

        window.display();
    }

    client.disconnect();
    shutdownEmbeddedServer();
    return 0;
}
