#pragma once
// The referee. It owns the two fighters and decides everything that involves
// both of them: who is in range of whom, who gets pushed out of an overlap,
// and when the match is over.
//
// A fighter's own state (health, stamina, what action it is in) belongs to
// Fighter and is changed only through the orders on that class.
//
// No sockets, no threads, no clock. Give it input, call step(dt). That is what
// makes the rules testable without playing a real match.

#include "server/sim/Fighter.h"
#include "common/Protocol.h"
#include <array>

class ArenaSim {
public:
    ArenaSim() { resetMatch(); }

    // Clear health, positions and the winner. Players who are in stay in.
    void resetMatch();

    void addPlayer(int id);
    void removePlayer(int id);
    bool active(int id) const;
    int  activeCount() const;

    void setInput(int id, const PlayerInput& in);
    void setPosition(int id, Vec2 pos);

    void step(float dt);

    bool matchOver() const { return winner_ != net::NO_WINNER; }
    uint8_t winner() const { return winner_; }

    const Fighter& player(int id) const { return fighters_[id]; }
    void fillSnapshots(net::PlayerSnapshot* out) const;

private:
    void resolveAttack(int attackerId);
    void separateFighters();

    std::array<Fighter, net::MAX_PLAYERS> fighters_;
    uint8_t winner_ = net::NO_WINNER;
};
