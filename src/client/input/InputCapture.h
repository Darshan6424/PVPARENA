#pragma once
// Reads the keyboard for one player. Attack/parry/dodge are reported only on
// the frame the key goes down; block is reported the whole time it's held.

#include <SFML/Window.hpp>

struct KeyBindings {
    sf::Keyboard::Key up, down, left, right;
    sf::Keyboard::Key attack, parry, dodge, block;
    bool allowMouse;   // player 1 can swing with the mouse too
};

struct InputSample {
    float moveX = 0.f;
    float moveY = 0.f;
    bool attack = false;
    bool block = false;
    bool parry = false;
    bool dodge = false;
};

class InputCapture {
public:
    explicit InputCapture(const KeyBindings& keys) : keys_(keys) {}

    InputSample sample();

    // Call when a match starts so a key held from the menu isn't read as a
    // fresh press.
    void reset();

private:
    KeyBindings keys_;
    bool prevAttack_ = false;
    bool prevParry_ = false;
    bool prevDodge_ = false;
};

extern const KeyBindings PLAYER1_KEYS;
extern const KeyBindings PLAYER2_KEYS;
