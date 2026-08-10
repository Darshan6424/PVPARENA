#pragma once
// Protocol.h
// Shared wire-format definitions for the client and server.
// Both sides #include this so packet layouts can never drift apart.

#include <cstdint>

namespace net {

// Bumped whenever the packet layout changes so mismatched builds don't
// silently misinterpret each other's bytes.
constexpr uint16_t PROTOCOL_MAGIC = 0xC0DE;

constexpr int   MAX_PLAYERS = 2;
constexpr float ARENA_WIDTH  = 900.f;
constexpr float ARENA_HEIGHT = 500.f;

constexpr float TICK_RATE = 60.f;              // server simulation rate
constexpr float TICK_DT   = 1.f / TICK_RATE;

constexpr uint16_t DEFAULT_SERVER_PORT = 9422;

// ---------------------------------------------------------------------
// Combat tuning constants. Server is authoritative on all of these; the
// client only uses them for small cosmetic prediction (see GameClient).
// ---------------------------------------------------------------------
constexpr float PLAYER_RADIUS   = 22.f;
constexpr float MOVE_SPEED      = 190.f;   // px/s while walking

constexpr float ATTACK_RANGE     = 70.f;   // center-to-center distance that lands a hit
constexpr float ATTACK_WINDUP    = 0.09f;  // seconds before the hit becomes active
constexpr float ATTACK_ACTIVE    = 0.08f;  // seconds the hit can actually land
constexpr float ATTACK_RECOVERY  = 0.28f;  // seconds of recovery after the swing
constexpr float ATTACK_DAMAGE    = 14.f;
constexpr float ATTACK_STAMINA_COST = 16.f;

constexpr float PARRY_WINDUP    = 0.05f;   // brief startup before the parry is "live"
constexpr float PARRY_WINDOW    = 0.18f;   // how long the parry is active/live
constexpr float PARRY_RECOVERY  = 0.35f;   // recovery if you whiff a parry
constexpr float PARRY_STAMINA_COST = 12.f;
constexpr float PARRY_STAGGER_DURATION = 0.6f; // stagger applied to whoever gets parried

constexpr float BLOCK_DAMAGE_MULT       = 0.35f; // damage still taken while blocking
constexpr float BLOCK_STAMINA_PER_SEC   = 22.f;  // stamina drained per second held

constexpr float DODGE_SPEED     = 520.f;
constexpr float DODGE_DURATION  = 0.16f;
constexpr float DODGE_STAMINA_COST = 18.f;
constexpr float DODGE_IFRAME_TIME  = 0.14f; // portion of the dodge that is invulnerable

constexpr float HIT_STAGGER_DURATION = 0.22f; // brief stagger on landing a normal hit

constexpr float MAX_STAMINA = 100.f;
constexpr float STAMINA_REGEN_PER_SEC   = 18.f;
constexpr float STAMINA_REGEN_DELAY     = 0.8f; // pause after spending stamina before it regens

constexpr float MAX_HEALTH = 100.f;

enum class PacketType : uint8_t {
    ConnectRequest = 1,
    ConnectAccept  = 2,
    ConnectReject  = 3,
    InputState     = 4,
    StateUpdate    = 5,
    Disconnect     = 6,
};

enum class ActionState : uint8_t {
    Idle       = 0,
    Moving     = 1,
    Attacking  = 2,
    Blocking   = 3,
    Parrying   = 4,
    Dodging    = 5,
    Staggered  = 6,
    Dead       = 7,
};

#pragma pack(push, 1)

struct PacketHeader {
    uint16_t   magic = PROTOCOL_MAGIC;
    PacketType type;
};

struct ConnectRequestPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PacketType::ConnectRequest };
};

struct ConnectAcceptPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PacketType::ConnectAccept };
    uint8_t assignedPlayerId = 0;
};

struct ConnectRejectPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PacketType::ConnectReject };
    char reason[64] = {};
};

// Client -> Server, sent once per client frame.
// "attack/parry/dodge" are edge-triggered (1 = pressed this frame),
// "block" is level-triggered (1 = currently held).
struct InputStatePacket {
    PacketHeader header{ PROTOCOL_MAGIC, PacketType::InputState };
    uint8_t  playerId = 0;
    uint32_t sequence = 0;
    float    moveX = 0.f; // -1..1
    float    moveY = 0.f; // -1..1
    uint8_t  attack = 0;
    uint8_t  block  = 0;
    uint8_t  parry  = 0;
    uint8_t  dodge  = 0;
};

struct PlayerSnapshot {
    float       x = 0.f, y = 0.f;
    float       facing = 1.f;     // +1 = facing right, -1 = facing left
    float       health = MAX_HEALTH;
    float       stamina = MAX_STAMINA;
    ActionState state = ActionState::Idle;
    uint8_t     connected = 0;
};

// Server -> Clients, broadcast every tick to both players.
struct StateUpdatePacket {
    PacketHeader   header{ PROTOCOL_MAGIC, PacketType::StateUpdate };
    uint32_t       tick = 0;
    uint8_t        winnerId = 255; // 255 = no winner yet, else player id who won
    PlayerSnapshot players[MAX_PLAYERS];
};

struct DisconnectPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PacketType::Disconnect };
    uint8_t playerId = 0;
};

#pragma pack(pop)

} // namespace net
