# PvP Arena

A simple server-authoritative 1v1 PvP duel game. C++17, SFML for
rendering, raw UDP for networking - no game frameworks.

Move around a small arena, land melee hits, parry your opponent's
swings, dodge through attacks, and block to reduce chip damage.
First to zero health loses.

## Controls

| Key                  | Action                          |
|-----------------------|----------------------------------|
| `W A S D`             | Move                             |
| `J` or Left Click     | Attack                           |
| `K` or Right Click    | Parry (short window - timing matters) |
| `Space`                | Dodge (brief invulnerability)   |
| `Left Shift` (hold)   | Block (reduces damage, drains stamina) |
| `Esc`                  | Return to title screen           |

## How the netcode works

The **server owns the entire game** - positions, health, stamina,
who's attacking, who parried whom. It ticks at a fixed 30Hz. The
**client never computes gameplay**; it just sends your input (move
direction + button presses) to the server and renders whatever the
server's last `StateUpdate` packet says. This means there's no
client-side prediction and no cheating-by-modified-client - if it
didn't happen on the server, it didn't happen.

- `ConnectRequest` → `ConnectAccept` (server assigns you player id 0 or 1)
- Client sends `InputState` every frame
- Server sends `StateUpdate` (both players' positions/health/stamina/state) every tick

You can either:
- **Create Server** - runs the server on a background thread inside
  your own client process, and connects you to `127.0.0.1`
  automatically. Good for testing solo (see below) or hosting.
- **Join** - type the host's IP address to connect to a server
  someone else is running (their own "Create Server", or a
  standalone `server` process - see below).

## Building

### Windows (VS Code + MSVC + vcpkg + CMake Tools)

```powershell
vcpkg install sfml
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Binaries land in `build/Release/` (or `build/` depending on your
generator): `client.exe`, `server.exe`, `headless_test.exe`.

> **This code targets SFML 3.x** (what current vcpkg installs pull by
> default). If your vcpkg instead gives you SFML 2.x, the APIs are
> different enough (event handling, `sf::Text`/`sf::FloatRect`
> constructors, scoped `Keyboard::Key`/`Mouse::Button` enums) that
> `client/Renderer.cpp` and `client/main_client.cpp` would need
> patching back - ping me with the compile errors and I'll adjust
> those two files.

### Linux

```bash
sudo apt install libsfml-dev cmake g++
cmake -B build -S .
cmake --build build -j4
```

Binaries land in `build/`: `client`, `server`, `headless_test`.

## Running

**Two players on the same machine (testing):** run `client`, click
**Create Server**, play as player 0. Run a second `client`, click
**Join**, type `127.0.0.1`, play as player 1.

**Two players on different machines:** one player runs `client` and
clicks **Create Server** (make sure UDP port `9422` is open/forwarded
on their network), then tells the other player their IP. The other
player runs `client`, clicks **Join**, and types that IP.

**Dedicated server (no player hosting from their own client):** run
`server.exe` (or `./server`) standalone on any reachable machine, then
both players **Join** its IP. Optional custom port: `server.exe 9500`.

## What's implemented vs. not yet

**Working:** movement, player-vs-player collision, melee attacks,
blocking, dodging with i-frames, parrying (punishes the attacker with
a stagger instead of landing damage), stamina costs/regen, health,
win/lose detection, title screen, create/join flow.

**Not yet built:** reconnect/timeout handling if a player's client
crashes mid-match, matchmaking codes (Join still takes a raw IP),
animation (players render as flat-colored circles with a facing
indicator, not sprites), sound.

## Project layout

```
common/    Shared wire protocol, UDP socket wrapper, small math helpers
server/    Authoritative simulation (GameServer) + standalone server.exe entry point
client/    SFML rendering, input capture, title screen, and networking client
test/      Headless test that spins up the real server and drives two fake
           clients through movement/attack/parry to sanity-check the combat
           math without needing a display - this is what was used to verify
           the logic below actually works before shipping it.
```

## Tuning combat feel

All the numbers that matter (attack damage, stamina costs, parry
window length, dodge i-frame duration, move speed, etc.) are one
block of named constants at the top of `common/Protocol.h`. Change a
number there, rebuild, and every rule that depends on it - server
combat resolution, nothing on the client - updates automatically.
