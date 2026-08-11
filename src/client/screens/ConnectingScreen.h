#pragma once
#include "client/screens/Screen.h"

// Waits for the handshake to finish, then hands over to MatchScreen. In local
// co-op it also brings the second player's connection up.
class ConnectingScreen : public Screen {
public:
    ConnectingScreen(Game& game, bool localCoop);

    void update(float dt) override;
    void draw(float dt) override;

private:
    void bringSecondPlayerOnline(float dt);

    bool localCoop_;
};
