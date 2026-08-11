#include "client/app/Game.h"
#include "client/screens/TitleScreen.h"
#include "client/screens/AddressScreen.h"
#include "client/screens/ConnectingScreen.h"
#include "common/PathUtils.h"
#include "common/Protocol.h"
#include <algorithm>
#include <cstdlib>

using namespace net;

namespace {

constexpr float SHAKE_DURATION = 0.18f;
constexpr float SHAKE_MAGNITUDE = 6.f;

struct Address {
    std::string ip;
    uint16_t port = DEFAULT_SERVER_PORT;
};

// Accepts "1.2.3.4" or "1.2.3.4:9500".
Address parseAddress(const std::string& text) {
    Address addr;
    std::size_t colon = text.find(':');
    if (colon == std::string::npos) {
        addr.ip = text;
        return addr;
    }
    addr.ip = text.substr(0, colon);
    int port = std::atoi(text.c_str() + colon + 1);
    if (port > 0 && port <= 65535) addr.port = static_cast<uint16_t>(port);
    return addr;
}

} // namespace

Game::Game()
    : window_(sf::VideoMode({ static_cast<unsigned>(ARENA_WIDTH),
                              static_cast<unsigned>(ARENA_HEIGHT) }), "PvP Arena"),
      assets_(paths::executableDir() + "assets/"),
      renderer_(assets_),
      audio_(assets_) {
    window_.setFramerateLimit(60);
    assets_.loadAll();
    audio_.init();
    screen_ = std::make_unique<TitleScreen>(*this);
}

void Game::changeScreen(std::unique_ptr<Screen> next) {
    nextScreen_ = std::move(next);
}

int Game::run() {
    sf::Clock clock;

    while (window_.isOpen()) {
        float dt = clock.restart().asSeconds();
        mouse_ = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));

        while (const std::optional event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window_.close();
                break;
            }
            screen_->handleEvent(*event);
        }
        // Swap here rather than inside handleEvent: a screen usually asks for
        // the change from its own event handler, and deleting it while it is
        // still running would leave that call standing on freed memory.
        if (nextScreen_) screen_ = std::move(nextScreen_);

        screen_->update(dt);
        if (nextScreen_) screen_ = std::move(nextScreen_);

        if (shake_ > 0.f) shake_ = std::max(0.f, shake_ - dt);

        window_.clear(sf::Color(20, 22, 28));
        screen_->draw(dt);
        window_.display();
    }

    client_.disconnect();
    clientP2_.disconnect();
    embedded_.stop();
    return 0;
}

void Game::showTitle() {
    changeScreen(std::make_unique<TitleScreen>(*this));
}

void Game::showAddressEntry() {
    changeScreen(std::make_unique<AddressScreen>(*this));
}

void Game::startHosting(bool localCoop) {
    localCoop_ = localCoop;
    hostAddress_.clear();

    if (!embedded_.start(DEFAULT_SERVER_PORT)) {
        status_ = "Could not start the server (is port " +
                  std::to_string(DEFAULT_SERVER_PORT) + " already in use?)";
        return;
    }

    if (!client_.beginConnect("127.0.0.1", DEFAULT_SERVER_PORT)) {
        status_ = "Could not open a local socket";
        embedded_.stop();
        return;
    }

    if (!localCoop) hostAddress_ = UdpSocket::getLocalIPAddress();

    resetMatchState();
    status_ = localCoop ? "Starting local match..." : "Starting server...";
    changeScreen(std::make_unique<ConnectingScreen>(*this, localCoop));
}

void Game::startJoining() {
    Address addr = parseAddress(addressText_);
    if (addr.ip.empty()) {
        status_ = "Enter an address first";
        return;
    }

    localCoop_ = false;
    hostAddress_.clear();

    if (!client_.beginConnect(addr.ip, addr.port)) {
        status_ = "Could not open a socket";
        showTitle();
        return;
    }

    resetMatchState();
    status_ = "Connecting to " + addr.ip + ":" + std::to_string(addr.port) + " ...";
    changeScreen(std::make_unique<ConnectingScreen>(*this, false));
}

void Game::leaveMatch() {
    client_.disconnect();
    clientP2_.disconnect();
    embedded_.stop();
    localCoop_ = false;
    showTitle();
}

void Game::resetMatchState() {
    watcher_.reset();
    renderer_.resetAnimationState();
    input1_.reset();
    input2_.reset();
    shake_ = 0.f;
}

void Game::shakeScreen() {
    shake_ = SHAKE_DURATION;
}

void Game::applyScreenShake(float) {
    if (shake_ <= 0.f) return;

    std::uniform_real_distribution<float> jitter(-1.f, 1.f);
    float amount = SHAKE_MAGNITUDE * (shake_ / SHAKE_DURATION);
    sf::View view = window_.getDefaultView();
    view.move({ jitter(rng_) * amount, jitter(rng_) * amount });
    window_.setView(view);
}
