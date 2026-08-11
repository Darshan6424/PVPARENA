#include "client/input/InputCapture.h"

using Key = sf::Keyboard::Key;

const KeyBindings PLAYER1_KEYS{
    Key::W, Key::S, Key::A, Key::D,
    Key::J, Key::K, Key::Space, Key::LShift,
    true
};

const KeyBindings PLAYER2_KEYS{
    Key::Up, Key::Down, Key::Left, Key::Right,
    Key::RControl, Key::Slash, Key::Enter, Key::RShift,
    false
};

void InputCapture::reset() {
    prevAttack_ = false;
    prevParry_ = false;
    prevDodge_ = false;
}

InputSample InputCapture::sample() {
    InputSample out;

    if (sf::Keyboard::isKeyPressed(keys_.left))  out.moveX -= 1.f;
    if (sf::Keyboard::isKeyPressed(keys_.right)) out.moveX += 1.f;
    if (sf::Keyboard::isKeyPressed(keys_.up))    out.moveY -= 1.f;
    if (sf::Keyboard::isKeyPressed(keys_.down))  out.moveY += 1.f;

    bool attackDown = sf::Keyboard::isKeyPressed(keys_.attack) ||
        (keys_.allowMouse && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left));
    bool parryDown = sf::Keyboard::isKeyPressed(keys_.parry) ||
        (keys_.allowMouse && sf::Mouse::isButtonPressed(sf::Mouse::Button::Right));
    bool dodgeDown = sf::Keyboard::isKeyPressed(keys_.dodge);

    out.block = sf::Keyboard::isKeyPressed(keys_.block);
    out.attack = attackDown && !prevAttack_;
    out.parry = parryDown && !prevParry_;
    out.dodge = dodgeDown && !prevDodge_;

    prevAttack_ = attackDown;
    prevParry_ = parryDown;
    prevDodge_ = dodgeDown;
    return out;
}
