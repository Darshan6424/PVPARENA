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

## Assets

`assets/` (copied next to the built exe automatically by CMake) has a
trimmed-down set pulled from your old Clash Royale project's asset
pack - not everything in it, just what fit this game:

- `sprites/characterSheet.png` / `skeletonSheet.png` - Universal LPC
  Spritesheet layout (64x64 tiles, 13 cols x 21 rows). Player 1 renders
  as the character sheet, player 2 as the skeleton, so the two fighters
  read as distinct at a glance without needing color-coding.
  Walk/slash/thrust/spellcast/hurt rows are mapped to
  Idle/Moving/Attacking/Parrying/Blocking/Staggered/Dead respectively -
  see the big comment at the top of `client/Renderer.h` for the exact
  row layout if you want to retune it.
- `sprites/bloodParticle.png` - small hit-impact splash, spawned
  client-side whenever it observes a player's health drop between two
  server updates (downscaled from the original 2048x2048 source).
- `audio/title_music.ogg`, `battle_music.ogg` - menu and in-match
  music, looped (re-encoded from the original mp3s to Ogg Vorbis,
  ~18MB -> ~6.5MB combined, same audible quality at game volume).
- `audio/attack_swing.ogg`, `parry_success.ogg`, `hit_land.ogg`,
  `block_hit.ogg`, `ui_click.ogg`, `match_end.ogg` - one-shot SFX,
  triggered by the client watching for the relevant change between
  consecutive `StateUpdate` packets (an attack starting, health
  dropping, a Staggered player who was mid-swing against an opponent
  who was Parrying, a button press, `winnerId` first appearing). None
  of this is the client deciding gameplay - it's reacting to what the
  server already reported.

**Skipped on purpose:** the old title screen graphic (you called it
out), the projectile/spell sprites (no ranged attacks here), the
tile/map assets (this game doesn't use tilemaps), and the
inventory/UI chrome (health/stamina bars are still simple drawn rects
for now). If you want any of those swapped in - or want a real
block/parry pose instead of the reused Thrust/Spellcast stances I
improvised from the sheet (there wasn't a dedicated guard pose in the
pack) - point me at the file or a replacement pack and I'll wire it
up.

**A note on the animation and audio API calls in this pass:** SFML
3's exact method names for texture/sound-buffer loading
(`loadFromFile` vs `openFromFile`) and music looping
(`setLooping` vs `setLoop`) weren't something I could verify against
a real SFML 3 install here - I went with my best-confidence read of
the 3.0 API and kept it consistent with what's already confirmed
working (`Font::openFromFile`). If the build throws errors in
`Renderer.cpp` or `AudioManager.cpp` about a missing member function,
send the log and it's a one-line rename to fix.

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

The host's own screen now shows its LAN IP directly on the "Waiting
for opponent..." screen, so there's no need to go digging through
`ipconfig`.

If Join just hangs on "Connecting..." until it times out, it's almost
always one of these:

- **Windows Firewall.** The first time `client.exe` runs and starts a
  server, Windows should prompt "Windows Defender Firewall has
  blocked some features of this app" - click **Allow access** (tick
  both Private and Public networks). If that prompt was dismissed or
  denied earlier, go to *Windows Security → Firewall & network
  protection → Allow an app through firewall* and add `client.exe`
  manually, with both boxes checked.
- **Wrong kind of IP.** For two machines on the **same Wi-Fi/LAN**,
  use the LAN IP shown on the host's screen (`192.168.x.x` or
  `10.x.x.x`). For machines on **different networks** (over the
  internet), the LAN IP won't be reachable at all - the host needs
  their **public IP** (search "what's my ip") plus a port-forwarding
  rule on their router forwarding UDP `9422` to their PC's LAN IP.
  There's no relay/NAT-punchthrough in this project, so internet play
  genuinely requires that forwarding step.
- **Different networks with CGNAT** (common on mobile hotspots and
  some ISPs) can make port forwarding impossible regardless of router
  settings - if forwarding doesn't work, that's likely why.

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
