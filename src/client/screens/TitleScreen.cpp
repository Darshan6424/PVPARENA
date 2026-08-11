#include "client/screens/TitleScreen.h"
#include "client/screens/EventHelpers.h"
#include "client/app/Game.h"
#include "common/Protocol.h"

using namespace net;

TitleScreen::TitleScreen(Game& game)
    : Screen(game),
      hostBtn_({ ARENA_WIDTH / 2.f - 110.f, 200.f }, { 220.f, 46.f }),
      joinBtn_({ ARENA_WIDTH / 2.f - 110.f, 256.f }, { 220.f, 46.f }),
      coopBtn_({ ARENA_WIDTH / 2.f - 110.f, 312.f }, { 220.f, 46.f }) {
    game_.audio().playMusic(Track::Title);
}

void TitleScreen::handleEvent(const sf::Event& event) {
    if (!screens::leftClick(event)) return;

    if (game_.hovering(hostBtn_)) {
        game_.audio().play(Sfx::UiClick);
        game_.startHosting(false);
    } else if (game_.hovering(joinBtn_)) {
        game_.audio().play(Sfx::UiClick);
        game_.showAddressEntry();
    } else if (game_.hovering(coopBtn_)) {
        game_.audio().play(Sfx::UiClick);
        game_.startHosting(true);
    }
}

void TitleScreen::draw(float) {
    Renderer& r = game_.renderer();
    sf::RenderWindow& w = game_.window();

    r.drawCenteredText(w, "PVP ARENA", ARENA_WIDTH / 2.f, 110.f, 36, sf::Color::White);
    r.drawButton(w, hostBtn_, "Create Server", game_.hovering(hostBtn_));
    r.drawButton(w, joinBtn_, "Join", game_.hovering(joinBtn_));
    r.drawButton(w, coopBtn_, "Local Co-op", game_.hovering(coopBtn_));
    r.drawCenteredText(w, "(2 players, 1 keyboard, same PC)", ARENA_WIDTH / 2.f,
                       coopBtn_.position.y + coopBtn_.size.y + 16.f, 12,
                       sf::Color(140, 140, 140));

    if (!game_.statusMessage().empty()) {
        r.drawCenteredText(w, game_.statusMessage(), ARENA_WIDTH / 2.f, 400.f, 14,
                           sf::Color(255, 150, 150));
    }
}
