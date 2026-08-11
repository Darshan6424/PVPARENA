#pragma once
#include "client/screens/Screen.h"
#include "client/app/MatchWatcher.h"

// The match itself: send input, turn snapshots into sound and effects, draw
// the arena. Also handles the win screen and the rematch key.
class MatchScreen : public Screen {
public:
    explicit MatchScreen(Game& game);

    void handleEvent(const sf::Event& event) override;
    void update(float dt) override;
    void draw(float dt) override;

private:
    bool matchOver() const;
    void sendLocalInput();
    void previewOwnSwing();
    void applyEvents(const MatchEvents& events);
    void drawWaitingForOpponent();

    bool wasOver_ = false;
};
