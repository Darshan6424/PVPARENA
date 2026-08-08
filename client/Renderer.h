#pragma once
// Renderer.h
// Pure presentation. Every draw call here reads from a StateUpdatePacket
// the server sent - it never decides positions, health, or outcomes
// itself. Uses SFML shapes/text only, so the game runs with zero asset
// files; swap in sprites later without touching any other file.

#include "../common/Protocol.h"
#include <SFML/Graphics.hpp>

class Renderer {
public:
    Renderer();

    // Tries a handful of common system font paths. If none are found,
    // text simply won't be drawn - shapes and bars still work fine.
    void loadFont();

    void drawArena(sf::RenderWindow& window);
    void drawPlayer(sf::RenderWindow& window, const net::PlayerSnapshot& snap,
                     uint8_t playerIndex, uint8_t localPlayerId);
    void drawHud(sf::RenderWindow& window, const net::StateUpdatePacket& state,
                 uint8_t localPlayerId);
    void drawCenteredText(sf::RenderWindow& window, const std::string& text,
                           float x, float y, unsigned int size, sf::Color color);

    bool fontLoaded() const { return fontLoaded_; }
    const sf::Font& font() const { return font_; }

private:
    sf::Font font_;
    bool fontLoaded_ = false;
};
