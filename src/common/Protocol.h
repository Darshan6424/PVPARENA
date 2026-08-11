#pragma once
// Wire format shared by client and server. Both sides include this so the
// packet layouts can't drift apart.
//
// TODO: structs go over the wire as raw bytes, so this assumes both ends are
// little-endian with IEEE floats. Fine for x86/ARM, would need packing if that
// ever stops being true.

#include <cstdint>

namespace net {

// Every packet starts with this. Bump PROTOCOL_VERSION on any layout change
// so an old build talking to a new one gets dropped instead of reading
// garbage off the wire.
constexpr uint16_t PROTOCOL_MAGIC   = 0xC0DE;
constexpr uint8_t  PROTOCOL_VERSION = 2;

constexpr int MAX_PLAYERS = 2;
constexpr uint8_t NO_WINNER = 255;

constexpr float ARENA_WIDTH  = 900.f;
constexpr float ARENA_HEIGHT = 500.f;

constexpr float TICK_RATE = 60.f;
constexpr float TICK_DT   = 1.f / TICK_RATE;

constexpr uint16_t DEFAULT_SERVER_PORT = 9422;

// Drop a player we haven't heard from in this long. Clients send input every
// frame while connected, so silence this long means they're gone.
constexpr float CLIENT_TIMEOUT = 8.f;

// Combat tuning. The server is authoritative on all of it.
constexpr float PLAYER_RADIUS = 22.f;
constexpr float MOVE_SPEED = 190.f;

constexpr float ATTACK_RANGE = 70.f;
constexpr float ATTACK_WINDUP = 0.09f;
constexpr float ATTACK_ACTIVE = 0.08f;   // how long the hit can land
constexpr float ATTACK_RECOVERY = 0.28f;
constexpr float ATTACK_DAMAGE = 14.f;
constexpr float ATTACK_STAMINA_COST = 16.f;

constexpr float PARRY_WINDUP = 0.05f;
constexpr float PARRY_WINDOW = 0.18f;    // how long the parry is live
constexpr float PARRY_RECOVERY = 0.35f;
constexpr float PARRY_STAMINA_COST = 12.f;
constexpr float PARRY_STAGGER_DURATION = 0.6f;

constexpr float BLOCK_DAMAGE_MULT = 0.35f;
constexpr float BLOCK_STAMINA_PER_SEC = 22.f;

constexpr float DODGE_SPEED = 520.f;
constexpr float DODGE_DURATION = 0.16f;
constexpr float DODGE_STAMINA_COST = 18.f;
constexpr float DODGE_IFRAME_TIME = 0.14f;

constexpr float HIT_STAGGER_DURATION = 0.22f;

constexpr float MAX_STAMINA = 100.f;
constexpr float STAMINA_REGEN_PER_SEC = 18.f;
constexpr float STAMINA_REGEN_DELAY = 0.8f;

constexpr float MAX_HEALTH = 100.f;

enum class PacketType : uint8_t {
    ConnectRequest = 1,
    ConnectAccept  = 2,
    ConnectReject  = 3,
    InputState     = 4,
    StateUpdate    = 5,
    Disconnect     = 6,
    Rematch        = 7,
};

enum class ActionState : uint8_t {
    Idle      = 0,
    Moving    = 1,
    Attacking = 2,
    Blocking  = 3,
    Parrying  = 4,
    Dodging   = 5,
    Staggered = 6,
    Dead      = 7,
};

#pragma pack(push, 1)

struct PacketHeader {
    uint16_t   magic = PROTOCOL_MAGIC;
    uint8_t    version = PROTOCOL_VERSION;
    PacketType type;
};

struct ConnectRequestPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PROTOCOL_VERSION, PacketType::ConnectRequest };
};

struct ConnectAcceptPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PROTOCOL_VERSION, PacketType::ConnectAccept };
    uint8_t assignedPlayerId = 0;
};

struct ConnectRejectPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PROTOCOL_VERSION, PacketType::ConnectReject };
    char reason[64] = {};
};

// Sent once per client frame. attack/parry/dodge are edge-triggered (1 means
// "pressed this frame"); block is level-triggered (1 means "held right now").
struct InputStatePacket {
    PacketHeader header{ PROTOCOL_MAGIC, PROTOCOL_VERSION, PacketType::InputState };
    uint8_t  playerId = 0;
    uint32_t sequence = 0;
    float    moveX = 0.f;
    float    moveY = 0.f;
    uint8_t  attack = 0;
    uint8_t  block = 0;
    uint8_t  parry = 0;
    uint8_t  dodge = 0;
};

struct PlayerSnapshot {
    float x = 0.f, y = 0.f;
    float facing = 1.f;
    float health = MAX_HEALTH;
    float stamina = MAX_STAMINA;
    ActionState state = ActionState::Idle;
    uint8_t connected = 0;
};

struct StateUpdatePacket {
    PacketHeader header{ PROTOCOL_MAGIC, PROTOCOL_VERSION, PacketType::StateUpdate };
    uint32_t tick = 0;
    uint8_t  winnerId = NO_WINNER;
    PlayerSnapshot players[MAX_PLAYERS];
};

struct DisconnectPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PROTOCOL_VERSION, PacketType::Disconnect };
    uint8_t playerId = 0;
};

// Sent from the game over screen. The server restarts once everyone asks.
struct RematchPacket {
    PacketHeader header{ PROTOCOL_MAGIC, PROTOCOL_VERSION, PacketType::Rematch };
    uint8_t playerId = 0;
};

#pragma pack(pop)

} // namespace net
