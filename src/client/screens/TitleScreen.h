#pragma once
#include "client/screens/Screen.h"

// Three buttons: host a server, join one, or play both sides locally.
class TitleScreen : public Screen {
public:
    explicit TitleScreen(Game& game);

    void handleEvent(const sf::Event& event) override;
    void draw(float dt) override;

private:
    sf::FloatRect hostBtn_;
    sf::FloatRect joinBtn_;
    sf::FloatRect coopBtn_;
};
