#pragma once
// Small readability helpers shared by the screens.

#include <SFML/Graphics.hpp>

namespace screens {

inline bool leftClick(const sf::Event& event) {
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    return click && click->button == sf::Mouse::Button::Left;
}

inline bool keyPressed(const sf::Event& event, sf::Keyboard::Key key) {
    const auto* pressed = event.getIf<sf::Event::KeyPressed>();
    return pressed && pressed->code == key;
}

} // namespace screens
