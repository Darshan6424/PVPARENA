// PvP Arena client.
//
// P1: WASD move, J or left click attack, K or right click parry, Space dodge,
// Left Shift hold to block. P2 (local co-op): arrows, RCtrl, /, Enter, RShift.
// R asks for a rematch on the win screen, Esc goes back to the title.

#include "client/app/Game.h"

int main() {
    Game game;
    return game.run();
}
