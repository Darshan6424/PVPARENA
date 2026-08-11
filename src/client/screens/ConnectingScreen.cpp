#include "client/screens/ConnectingScreen.h"
#include "client/screens/MatchScreen.h"
#include "client/app/Game.h"
#include "common/Protocol.h"
#include <memory>

using namespace net;

ConnectingScreen::ConnectingScreen(Game& game, bool localCoop)
    : Screen(game), localCoop_(localCoop) {}

void ConnectingScreen::update(float dt) {
    GameClient& client = game_.client();
    client.update(dt);

    switch (client.status()) {
    case ConnectionStatus::Rejected:
        game_.setStatusMessage(client.rejectReason().empty()
                                   ? "Server rejected the connection"
                                   : client.rejectReason());
        game_.leaveMatch();
        return;
    case ConnectionStatus::TimedOut:
        game_.setStatusMessage("Connection timed out");
        game_.leaveMatch();
        return;
    case ConnectionStatus::Disconnected:
        game_.setStatusMessage("Connection failed");
        game_.leaveMatch();
        return;
    case ConnectionStatus::Connecting:
        return;
    case ConnectionStatus::Connected:
        break;
    }

    if (!localCoop_) {
        game_.setStatusMessage("");
        game_.changeScreen(std::make_unique<MatchScreen>(game_));
        return;
    }
    bringSecondPlayerOnline(dt);
}

void ConnectingScreen::bringSecondPlayerOnline(float dt) {
    GameClient& p2 = game_.secondClient();

    if (p2.status() == ConnectionStatus::Disconnected) {
        if (!p2.beginConnect("127.0.0.1", DEFAULT_SERVER_PORT)) {
            game_.setStatusMessage("Could not start the second local player");
            game_.leaveMatch();
            return;
        }
    }
    p2.update(dt);

    if (p2.status() == ConnectionStatus::Connected) {
        game_.setStatusMessage("");
        game_.changeScreen(std::make_unique<MatchScreen>(game_));
    } else if (p2.status() == ConnectionStatus::Rejected ||
               p2.status() == ConnectionStatus::TimedOut) {
        game_.setStatusMessage("Could not start the second local player");
        game_.leaveMatch();
    }
}

void ConnectingScreen::draw(float) {
    game_.renderer().drawCenteredText(game_.window(), game_.statusMessage(),
                                      ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f, 20,
                                      sf::Color::White);
}
