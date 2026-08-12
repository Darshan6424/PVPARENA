#pragma once
#include "client/screens/Screen.h"

// Types in the address of a server to join. Accepts an IP or a domain name,
// either on its own or with ":port" appended.
class AddressScreen : public Screen {
public:
    explicit AddressScreen(Game& game);

    void handleEvent(const sf::Event& event) override;
    void draw(float dt) override;

private:
    void typeCharacter(char32_t c);

    sf::FloatRect connectBtn_;
    sf::FloatRect backBtn_;
};
