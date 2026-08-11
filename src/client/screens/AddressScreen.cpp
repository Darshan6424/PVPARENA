#include "client/screens/AddressScreen.h"
#include "client/screens/EventHelpers.h"
#include "client/app/Game.h"
#include "common/Protocol.h"

using namespace net;

AddressScreen::AddressScreen(Game& game)
    : Screen(game),
      connectBtn_({ ARENA_WIDTH / 2.f - 90.f, 300.f }, { 180.f, 44.f }),
      backBtn_({ 20.f, 20.f }, { 90.f, 36.f }) {}

void AddressScreen::handleEvent(const sf::Event& event) {
    if (screens::keyPressed(event, sf::Keyboard::Key::Escape)) {
        game_.showTitle();
        return;
    }

    if (const auto* typed = event.getIf<sf::Event::TextEntered>()) {
        typeCharacter(typed->unicode);
        return;
    }

    if (!screens::leftClick(event)) return;

    if (game_.hovering(connectBtn_)) {
        game_.audio().play(Sfx::UiClick);
        game_.startJoining();
    } else if (game_.hovering(backBtn_)) {
        game_.audio().play(Sfx::UiClick);
        game_.showTitle();
    }
}

void AddressScreen::typeCharacter(char32_t c) {
    std::string& text = game_.addressText();

    if (c == 8) {                       // backspace
        if (!text.empty()) text.pop_back();
    } else if (c == 13) {               // enter
        game_.startJoining();
    } else if (c < 128) {
        char ch = static_cast<char>(c);
        bool allowed = (ch >= '0' && ch <= '9') || ch == '.' || ch == ':';
        if (allowed && text.size() < 45) text += ch;
    }
}

void AddressScreen::draw(float) {
    Renderer& r = game_.renderer();
    sf::RenderWindow& w = game_.window();

    r.drawTitleBackground(w);
    r.drawCenteredText(w, "Enter server address", ARENA_WIDTH / 2.f, 190.f, 24,
                       sf::Color::White);
    r.drawCenteredText(w, game_.addressText(), ARENA_WIDTH / 2.f, 240.f, 28,
                       sf::Color(120, 220, 255));
    r.drawCenteredText(w, "IP, or IP:port for a non-default port",
                       ARENA_WIDTH / 2.f, 270.f, 12, sf::Color(140, 140, 140));
    r.drawButton(w, connectBtn_, "Connect", game_.hovering(connectBtn_));
    r.drawButton(w, backBtn_, "Back", game_.hovering(backBtn_));
}
