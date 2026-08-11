#pragma once
// Base class for the four things the window can be showing: the title screen,
// the address entry screen, the connecting screen and the match itself.
//
// The main loop does not know or care which one is active. It calls the same
// three methods every frame and the right behaviour happens - that is the
// point of making these virtual.

#include <SFML/Graphics.hpp>

class Game;

class Screen {
public:
    explicit Screen(Game& game) : game_(game) {}
    virtual ~Screen() = default;

    // No copying: a screen is tied to the Game it was built for.
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    virtual void handleEvent(const sf::Event& event) { (void)event; }
    virtual void update(float dt) { (void)dt; }
    virtual void draw(float dt) = 0;

protected:
    Game& game_;
};
