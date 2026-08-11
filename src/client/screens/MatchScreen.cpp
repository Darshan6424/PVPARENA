#include "client/screens/MatchScreen.h"
#include "client/screens/EventHelpers.h"
#include "client/app/Game.h"
#include "common/Protocol.h"

using namespace net;

MatchScreen::MatchScreen(Game& game) : Screen(game) {
    game_.audio().playMusic(Track::Battle);
}

bool MatchScreen::matchOver() const {
    const GameClient& client = game_.client();
    return client.hasReceivedState() && client.latestState().winnerId != NO_WINNER;
}

void MatchScreen::handleEvent(const sf::Event& event) {
    if (screens::keyPressed(event, sf::Keyboard::Key::Escape)) {
        game_.leaveMatch();
        return;
    }
    if (screens::keyPressed(event, sf::Keyboard::Key::R) && matchOver()) {
        game_.client().sendRematch();
        if (game_.isLocalCoop()) game_.secondClient().sendRematch();
    }
}

void MatchScreen::update(float dt) {
    GameClient& client = game_.client();
    client.update(dt);
    if (game_.isLocalCoop()) game_.secondClient().update(dt);

    if (client.status() != ConnectionStatus::Connected) {
        game_.setStatusMessage("Lost connection to the server");
        game_.leaveMatch();
        return;
    }

    bool over = matchOver();
    sendLocalInput();

    if (wasOver_ && !over) {            // a rematch just started
        game_.resetMatchState();
    }
    wasOver_ = over;

    if (client.hasReceivedState()) {
        applyEvents(game_.watcher().observe(client.latestState(), dt));
    }
}

void MatchScreen::sendLocalInput() {
    GameClient& client = game_.client();

    if (matchOver()) {
        // Keep the connection alive while the win screen is up, or the server
        // times us out before we can ask for a rematch.
        client.sendInput(0.f, 0.f, false, false, false, false);
        if (game_.isLocalCoop()) {
            game_.secondClient().sendInput(0.f, 0.f, false, false, false, false);
        }
        return;
    }

    InputSample p1 = game_.player1Input().sample();
    if (p1.attack) previewOwnSwing();
    client.sendInput(p1.moveX, p1.moveY, p1.attack, p1.block, p1.parry, p1.dodge);

    if (game_.isLocalCoop()) {
        InputSample p2 = game_.player2Input().sample();
        game_.secondClient().sendInput(p2.moveX, p2.moveY, p2.attack, p2.block,
                                       p2.parry, p2.dodge);
    }
}

void MatchScreen::previewOwnSwing() {
    // Play our own swing sound straight away instead of waiting for the round
    // trip. Sound only - the server still decides what actually happens.
    const GameClient& client = game_.client();
    uint8_t id = client.localPlayerId();
    if (id >= MAX_PLAYERS || !client.hasReceivedState()) return;

    const PlayerSnapshot& me = client.latestState().players[id];
    bool busy = me.state == ActionState::Attacking || me.state == ActionState::Parrying ||
                me.state == ActionState::Dodging   || me.state == ActionState::Staggered ||
                me.state == ActionState::Dead;

    if (!busy && me.stamina >= ATTACK_STAMINA_COST) {
        game_.audio().play(Sfx::AttackSwing);
        game_.watcher().muteNextAttackSound(id);
    }
}

void MatchScreen::applyEvents(const MatchEvents& events) {
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const PlayerEvents& e = events.players[i];
        if (e.startedAttack) game_.audio().play(Sfx::AttackSwing);
        if (e.gotParried) game_.audio().play(Sfx::ParrySuccess);
        if (e.tookBlockedHit) game_.audio().play(Sfx::BlockHit);
        if (e.tookCleanHit) {
            game_.audio().play(Sfx::HitLand);
            game_.shakeScreen();
        }
        if (e.tookCleanHit || e.tookBlockedHit) {
            game_.renderer().spawnHitEffect(e.hitX, e.hitY);
            game_.renderer().spawnDamageNumber(e.hitX, e.hitY - 45.f, e.damage,
                                               e.tookBlockedHit);
        }
    }
    if (events.matchJustEnded) game_.audio().play(Sfx::MatchEnd);
}

void MatchScreen::draw(float dt) {
    Renderer& r = game_.renderer();
    sf::RenderWindow& w = game_.window();
    const GameClient& client = game_.client();

    game_.applyScreenShake(dt);
    r.drawArena(w);

    uint8_t localId = client.localPlayerId();
    bool opponentHere = localId < MAX_PLAYERS && client.hasReceivedState() &&
                        client.latestState().players[1 - localId].connected;

    if (!opponentHere) {
        drawWaitingForOpponent();
        w.setView(w.getDefaultView());
        return;
    }

    const StateUpdatePacket& state = client.latestState();
    uint8_t povId = game_.isLocalCoop() ? NO_WINNER : localId;
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        r.drawPlayer(w, state.players[i], i, povId, game_.watcher().stateTime(i));
    }
    r.updateAndDrawEffects(w, dt);
    r.drawHud(w, state, povId);

    if (game_.isLocalCoop() && !matchOver()) {
        r.drawCenteredText(w, "P1: WASD / J attack / K parry / Space dodge / Shift block",
                           ARENA_WIDTH / 2.f, ARENA_HEIGHT - 28.f, 12,
                           sf::Color(150, 190, 255));
        r.drawCenteredText(w, "P2: Arrows / RCtrl attack / \"/\" parry / Enter dodge / RShift block",
                           ARENA_WIDTH / 2.f, ARENA_HEIGHT - 12.f, 12,
                           sf::Color(255, 170, 150));
    }

    w.setView(w.getDefaultView());
}

void MatchScreen::drawWaitingForOpponent() {
    Renderer& r = game_.renderer();
    sf::RenderWindow& w = game_.window();

    r.drawCenteredText(w, "Waiting for opponent to join...", ARENA_WIDTH / 2.f,
                       ARENA_HEIGHT / 2.f - 60.f, 22, sf::Color::White);

    if (!game_.hostAddress().empty()) {
        sf::RectangleShape card({ 360.f, 90.f });
        card.setOrigin({ 180.f, 45.f });
        card.setPosition({ ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f + 10.f });
        card.setFillColor(sf::Color(30, 32, 40));
        card.setOutlineColor(sf::Color(120, 200, 255));
        card.setOutlineThickness(2.f);
        w.draw(card);

        r.drawCenteredText(w, "Have your opponent Join this:", ARENA_WIDTH / 2.f,
                           ARENA_HEIGHT / 2.f - 12.f, 14, sf::Color(180, 180, 180));
        r.drawCenteredText(w, game_.hostAddress() + ":" + std::to_string(DEFAULT_SERVER_PORT),
                           ARENA_WIDTH / 2.f, ARENA_HEIGHT / 2.f + 20.f, 24,
                           sf::Color(150, 220, 255));
    }

    r.drawCenteredText(w, "Press ESC to cancel", ARENA_WIDTH / 2.f,
                       ARENA_HEIGHT / 2.f + 110.f, 13, sf::Color(140, 140, 140));
}
