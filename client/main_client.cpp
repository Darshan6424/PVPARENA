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
#include "AudioManager.h"
#include "../server/GameServer.h"
#include "../common/Protocol.h"
#include "../common/PathUtils.h"
#include <SFML/Graphics.hpp>
#include <optional>
#include <atomic>
#include <thread>
#include <memory>
#include <cctype>
#include <array>
#include <cstdlib>
#include <ctime>

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
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode({ static_cast<unsigned>(ARENA_WIDTH),
                                             static_cast<unsigned>(ARENA_HEIGHT) }),
                             "PvP Arena");
    window.setFramerateLimit(60);

    Renderer renderer;
    std::string assetsDir = paths::executableDir() + "assets/";
    renderer.loadTextures(assetsDir);

    AudioManager audio;
    audio.loadAll(assetsDir);
    audio.playMusic(MusicTrack::Title);

    GameClient client;
    GameClient clientP2; // only used when isLocalCoop is true
    bool isLocalCoop = false;
    AppState appState = AppState::Title;

    std::string ipInput = "127.0.0.1";
    std::string statusMessage;
    std::string hostIpDisplay;

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

    sf::FloatRect createBtn({ ARENA_WIDTH / 2.f - 110.f, 200.f }, { 220.f, 46.f });
    sf::FloatRect joinBtn({ ARENA_WIDTH / 2.f - 110.f, 256.f }, { 220.f, 46.f });
    sf::FloatRect coopBtn({ ARENA_WIDTH / 2.f - 110.f, 312.f }, { 220.f, 46.f });
    sf::FloatRect connectBtn({ ARENA_WIDTH / 2.f - 90.f, 300.f }, { 180.f, 44.f });
    sf::FloatRect backBtn({ 20.f, 20.f }, { 90.f, 36.f });

    struct KeyBindings {
        sf::Keyboard::Key up, down, left, right;
        sf::Keyboard::Key attack, parry, dodge, block;
        bool allowMouse = false; // P1 in solo modes can also use the mouse
    };
    struct InputEdgeState {
        bool prevAttack = false, prevParry = false, prevDodge = false;
    };

    const KeyBindings p1Bindings{
        sf::Keyboard::Key::W, sf::Keyboard::Key::S, sf::Keyboard::Key::A, sf::Keyboard::Key::D,
        sf::Keyboard::Key::J, sf::Keyboard::Key::K, sf::Keyboard::Key::Space, sf::Keyboard::Key::LShift,
        true
    };
    const KeyBindings p2Bindings{
        sf::Keyboard::Key::Up, sf::Keyboard::Key::Down, sf::Keyboard::Key::Left, sf::Keyboard::Key::Right,
        sf::Keyboard::Key::RControl, sf::Keyboard::Key::Slash, sf::Keyboard::Key::Enter, sf::Keyboard::Key::RShift,
        false
    };

    InputEdgeState p1Edge, p2Edge;

    auto captureInput = [](const KeyBindings& kb, InputEdgeState& edge,
                            float& moveX, float& moveY, bool& attackPressed,
                            bool& blockHeld, bool& parryPressed, bool& dodgePressed) {
        moveX = 0.f; moveY = 0.f;
        if (sf::Keyboard::isKeyPressed(kb.left))  moveX -= 1.f;
        if (sf::Keyboard::isKeyPressed(kb.right)) moveX += 1.f;
        if (sf::Keyboard::isKeyPressed(kb.up))    moveY -= 1.f;
        if (sf::Keyboard::isKeyPressed(kb.down))  moveY += 1.f;

        bool attackHeld = sf::Keyboard::isKeyPressed(kb.attack) ||
            (kb.allowMouse && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left));
        bool parryHeld = sf::Keyboard::isKeyPressed(kb.parry) ||
            (kb.allowMouse && sf::Mouse::isButtonPressed(sf::Mouse::Button::Right));
        bool dodgeHeld = sf::Keyboard::isKeyPressed(kb.dodge);
        blockHeld = sf::Keyboard::isKeyPressed(kb.block);

        attackPressed = attackHeld && !edge.prevAttack;
        parryPressed  = parryHeld  && !edge.prevParry;
        dodgePressed  = dodgeHeld  && !edge.prevDodge;
        edge.prevAttack = attackHeld;
        edge.prevParry  = parryHeld;
        edge.prevDodge  = dodgeHeld;
    };

    std::array<PlayerSnapshot, MAX_PLAYERS> prevSnapshots{};
    bool havePrevSnapshots = false;
    std::array<float, MAX_PLAYERS> stateTimer{ 0.f, 0.f };
    std::array<bool, MAX_PLAYERS> suppressNextAttackSound{ false, false };
    bool matchEndSoundPlayed = false;
    float screenShakeTimer = 0.f;
    const float screenShakeDuration = 0.18f;

    auto resetMatchTracking = [&]() {
        havePrevSnapshots = false;
        stateTimer = { 0.f, 0.f };
        suppressNextAttackSound = { false, false };
        matchEndSoundPlayed = false;
        screenShakeTimer = 0.f;
    };

    // Instant local feedback: play the swing sound the moment a local
    // player presses attack, rather than waiting for the server to
    // confirm it - masks network/tick latency for the person who
    // pressed the button. Only fires if we can see (from the last
    // server snapshot) that the attack would actually be allowed to
    // start, to avoid a sound with no attack behind it. The real
    // outcome always still comes from the server; this never decides
    // anything, it just previews what's about to happen.
    auto tryOptimisticSwing = [&](uint8_t playerId, bool attackPressed) {
        if (!attackPressed || playerId >= MAX_PLAYERS || !client.hasReceivedState()) return;
        const PlayerSnapshot& snap = client.latestState().players[playerId];
        bool locked = (snap.state == ActionState::Attacking || snap.state == ActionState::Parrying ||
                       snap.state == ActionState::Dodging || snap.state == ActionState::Staggered ||
                       snap.state == ActionState::Dead);
        if (!locked && snap.stamina >= ATTACK_STAMINA_COST) {
            audio.playAttackSwing();
            suppressNextAttackSound[playerId] = true;
        }
    };

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
                            audio.playUiClick();
                            isLocalCoop = false;
                            embeddedServer = std::make_unique<GameServer>();
                            if (embeddedServer->start(DEFAULT_SERVER_PORT)) {
                                serverRunning = true;
                                serverThread = std::make_unique<std::thread>(
                                    [&]() { embeddedServer->run(serverRunning); });
                                hostIpDisplay = UdpSocket::getLocalIPAddress();
                                client.beginConnect("127.0.0.1", DEFAULT_SERVER_PORT);
                                resetMatchTracking();
                                appState = AppState::Connecting;
                                statusMessage = "Starting server and connecting...";
                            } else {
                                statusMessage = "Failed to start server (port in use?)";
                            }
                        } else if (buttonContains(joinBtn, mousePos)) {
                            audio.playUiClick();
                            isLocalCoop = false;
                            appState = AppState::EnteringIp;
                        } else if (buttonContains(coopBtn, mousePos)) {
                            audio.playUiClick();
                            isLocalCoop = true;
                            hostIpDisplay.clear();
                            embeddedServer = std::make_unique<GameServer>();
                            if (embeddedServer->start(DEFAULT_SERVER_PORT)) {
                                serverRunning = true;
                                serverThread = std::make_unique<std::thread>(
                                    [&]() { embeddedServer->run(serverRunning); });
                                client.beginConnect("127.0.0.1", DEFAULT_SERVER_PORT);
                                resetMatchTracking();
                                appState = AppState::Connecting;
                                statusMessage = "Starting local match...";
                            } else {
                                statusMessage = "Failed to start server (port in use?)";
                            }
                        }
                    }
                }
            } else if (appState == AppState::EnteringIp) {
                if (const auto* te = event->getIf<sf::Event::TextEntered>()) {
                    if (te->unicode == 8) { // backspace
                        if (!ipInput.empty()) ipInput.pop_back();
                    } else if (te->unicode == 13) { // enter
                        isLocalCoop = false;
                        client.beginConnect(ipInput, DEFAULT_SERVER_PORT);
                        resetMatchTracking();
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
                            audio.playUiClick();
                            isLocalCoop = false;
                            client.beginConnect(ipInput, DEFAULT_SERVER_PORT);
                            resetMatchTracking();
                            appState = AppState::Connecting;
                            statusMessage = "Connecting to " + ipInput + " ...";
                        } else if (buttonContains(backBtn, mousePos)) {
                            audio.playUiClick();
                            appState = AppState::Title;
                        }
                    }
                }
            }

            if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                if (kp->code == sf::Keyboard::Key::Escape) {
                    if (appState == AppState::Playing || appState == AppState::GameOver) {
                        client.disconnect();
                        if (isLocalCoop) clientP2.disconnect();
                        shutdownEmbeddedServer();
                        isLocalCoop = false;
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

            if (client.status() == ConnectionStatus::Rejected) {
                statusMessage = "Server rejected connection (full?)";
                shutdownEmbeddedServer();
                appState = AppState::Title;
            } else if (client.status() == ConnectionStatus::TimedOut) {
                statusMessage = "Connection timed out";
                shutdownEmbeddedServer();
                appState = AppState::Title;
            } else if (client.status() == ConnectionStatus::Connected) {
                if (!isLocalCoop) {
                    appState = AppState::Playing;
                } else {
                    // P1 is in; now bring P2's local connection online too.
                    if (clientP2.status() == ConnectionStatus::Disconnected) {
                        clientP2.beginConnect("127.0.0.1", DEFAULT_SERVER_PORT);
                    }
                    clientP2.update(dt);

                    if (clientP2.status() == ConnectionStatus::Connected) {
                        appState = AppState::Playing;
                    } else if (clientP2.status() == ConnectionStatus::Rejected ||
                               clientP2.status() == ConnectionStatus::TimedOut) {
                        statusMessage = "Could not start the second local player";
                        client.disconnect();
                        shutdownEmbeddedServer();
                        isLocalCoop = false;
                        appState = AppState::Title;
                    }
                }
            }
        }

        // ---- Playing: capture input, send it, pump state --------------
        if (appState == AppState::Playing || appState == AppState::GameOver) {
            client.update(dt);
            if (isLocalCoop) clientP2.update(dt);

            if (appState == AppState::Playing) {
                float p1MoveX, p1MoveY;
                bool p1Attack, p1Block, p1Parry, p1Dodge;
                captureInput(p1Bindings, p1Edge, p1MoveX, p1MoveY, p1Attack, p1Block, p1Parry, p1Dodge);
                tryOptimisticSwing(client.localPlayerId(), p1Attack);
                client.sendInput(p1MoveX, p1MoveY, p1Attack, p1Block, p1Parry, p1Dodge);

                if (isLocalCoop) {
                    float p2MoveX, p2MoveY;
                    bool p2Attack, p2Block, p2Parry, p2Dodge;
                    captureInput(p2Bindings, p2Edge, p2MoveX, p2MoveY, p2Attack, p2Block, p2Parry, p2Dodge);
                    tryOptimisticSwing(clientP2.localPlayerId(), p2Attack);
                    clientP2.sendInput(p2MoveX, p2MoveY, p2Attack, p2Block, p2Parry, p2Dodge);
                }
            }

            if (client.hasReceivedState()) {
                const StateUpdatePacket& state = client.latestState();

                for (int i = 0; i < MAX_PLAYERS; ++i) {
                    const PlayerSnapshot& snap = state.players[i];

                    if (havePrevSnapshots) {
                        const PlayerSnapshot& prev = prevSnapshots[i];

                        if (prev.state != snap.state) {
                            stateTimer[i] = 0.f;
                        } else {
                            stateTimer[i] += dt;
                        }

                        // Swing sound the instant an attack starts - unless we
                        // already played it optimistically the moment the
                        // local player pressed attack (see tryOptimisticSwing).
                        if (snap.state == ActionState::Attacking &&
                            prev.state != ActionState::Attacking) {
                            if (suppressNextAttackSound[i]) {
                                suppressNextAttackSound[i] = false;
                            } else {
                                audio.playAttackSwing();
                            }
                        }

                        // A player who was Attacking and got Staggered while
                        // their opponent was Parrying just got punished for
                        // swinging into a parry.
                        if (snap.state == ActionState::Staggered &&
                            prev.state == ActionState::Attacking &&
                            prevSnapshots[1 - i].state == ActionState::Parrying) {
                            audio.playParrySuccess();
                        }

                        // Health dropped since the last snapshot - something
                        // landed. Distinguish a blocked (reduced-damage) hit
                        // from a clean one for which sound to play.
                        if (snap.health < prev.health - 0.01f) {
                            if (snap.state == ActionState::Blocking ||
                                prev.state == ActionState::Blocking) {
                                audio.playBlockHit();
                            } else {
                                audio.playHitLand();
                                screenShakeTimer = screenShakeDuration;
                            }
                            renderer.spawnHitEffect({ snap.x, snap.y });
                        }
                    } else {
                        stateTimer[i] = 0.f;
                    }
                }

                for (int i = 0; i < MAX_PLAYERS; ++i) {
                    prevSnapshots[i] = state.players[i];
                }
                havePrevSnapshots = true;

                if (state.winnerId != 255 && !matchEndSoundPlayed) {
                    audio.playMatchEnd();
                    matchEndSoundPlayed = true;
                }
            }

            if (client.hasReceivedState() && client.latestState().winnerId != 255) {
                appState = AppState::GameOver;
            }
        }

        // ---- Music follows which screen we're on -----------------------
        bool inMatch = (appState == AppState::Playing || appState == AppState::GameOver);
        audio.playMusic(inMatch ? MusicTrack::Battle : MusicTrack::Title);

        if (screenShakeTimer > 0.f) {
            screenShakeTimer = std::max(0.f, screenShakeTimer - dt);
        }

        // ---------------------------------------------------------------
        // Draw
        // ---------------------------------------------------------------
        window.clear(sf::Color(20, 22, 28));

        if (appState == AppState::Title) {
            renderer.drawCenteredText(window, "PVP ARENA", ARENA_WIDTH / 2.f, 110.f, 36,
                                       sf::Color::White);
            bool hoverCreate = buttonContains(createBtn, mousePos);
            bool hoverJoin = buttonContains(joinBtn, mousePos);
            bool hoverCoop = buttonContains(coopBtn, mousePos);
            drawButton(window, renderer.font(), renderer.fontLoaded(), createBtn, "Create Server", hoverCreate);
            drawButton(window, renderer.font(), renderer.fontLoaded(), joinBtn, "Join", hoverJoin);
            drawButton(window, renderer.font(), renderer.fontLoaded(), coopBtn, "Local Co-op", hoverCoop);
            renderer.drawCenteredText(window, "(2 players, 1 keyboard, same PC)",
                                       ARENA_WIDTH / 2.f, coopBtn.position.y + coopBtn.size.y + 16.f, 12,
                                       sf::Color(140, 140, 140));
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
            if (screenShakeTimer > 0.f) {
                float magnitude = 6.f * (screenShakeTimer / screenShakeDuration);
                float offsetX = ((std::rand() % 200) / 100.f - 1.f) * magnitude;
                float offsetY = ((std::rand() % 200) / 100.f - 1.f) * magnitude;
                sf::View shakeView = window.getDefaultView();
                shakeView.move({ offsetX, offsetY });
                window.setView(shakeView);
            }

            renderer.drawArena(window);

            bool haveLocalId = client.localPlayerId() < MAX_PLAYERS;
            bool opponentConnected = haveLocalId && client.hasReceivedState() &&
                client.latestState().players[1 - client.localPlayerId()].connected;

            if (haveLocalId && client.hasReceivedState() && opponentConnected) {
                const StateUpdatePacket& state = client.latestState();
                uint8_t povId = isLocalCoop ? 255 : client.localPlayerId();
                for (int i = 0; i < MAX_PLAYERS; ++i) {
                    renderer.drawPlayer(window, state.players[i], static_cast<uint8_t>(i),
                                         povId, stateTimer[i]);
                }
                renderer.updateAndDrawHitEffects(window, dt);
                renderer.drawHud(window, state, povId);

                if (isLocalCoop && appState == AppState::Playing) {
                    renderer.drawCenteredText(window, "P1: WASD move / J attack / K parry / Space dodge / Shift block",
                                               ARENA_WIDTH / 2.f, ARENA_HEIGHT - 28.f, 12,
                                               sf::Color(150, 190, 255));
                    renderer.drawCenteredText(window, "P2: Arrows move / RCtrl attack / \"/\" parry / Enter dodge / RShift block",
                                               ARENA_WIDTH / 2.f, ARENA_HEIGHT - 12.f, 12,
                                               sf::Color(255, 170, 150));
                }
            } else {
                renderer.drawCenteredText(window, "Waiting for opponent to join...",
                                           ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f - 60.f, 22,
                                           sf::Color::White);

                if (embeddedServer && !hostIpDisplay.empty()) {
                    sf::RectangleShape card(sf::Vector2f(360.f, 90.f));
                    card.setOrigin({ 180.f, 45.f });
                    card.setPosition({ ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f + 10.f });
                    card.setFillColor(sf::Color(30, 32, 40));
                    card.setOutlineColor(sf::Color(120, 200, 255));
                    card.setOutlineThickness(2.f);
                    window.draw(card);

                    renderer.drawCenteredText(window, "Have your opponent Join this:",
                                               ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f - 12.f, 14,
                                               sf::Color(180, 180, 180));
                    renderer.drawCenteredText(window,
                                               hostIpDisplay + "  (port " +
                                                   std::to_string(DEFAULT_SERVER_PORT) + ")",
                                               ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f + 20.f, 24,
                                               sf::Color(150, 220, 255));
                }

                renderer.drawCenteredText(window, "Press ESC to cancel",
                                           ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f + 110.f, 13,
                                           sf::Color(140, 140, 140));
            }

            window.setView(window.getDefaultView());
        }

        window.display();
    }

    client.disconnect();
    if (isLocalCoop) clientP2.disconnect();
    shutdownEmbeddedServer();
    return 0;
}
