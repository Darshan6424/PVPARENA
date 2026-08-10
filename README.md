# PvP Arena

A simple server-authoritative 1v1 PvP duel game. C++17, SFML for
rendering, raw UDP for networking - no game frameworks.

Move around a small arena, land melee hits, parry your opponent's
swings, dodge through attacks, and block to reduce chip damage.
First to zero health loses.

## Controls

**Player 1 (or solo):**

| Key                  | Action                          |
|-----------------------|----------------------------------|
| `W A S D`             | Move                             |
| `J` or Left Click     | Attack                           |
| `K` or Right Click    | Parry (short window - timing matters) |
| `Space`                | Dodge (brief invulnerability)   |
| `Left Shift` (hold)   | Block (reduces damage, drains stamina) |
| `Esc`                  | Return to title screen           |

**Player 2 (Local Co-op only):**

| Key                  | Action                          |
|-----------------------|----------------------------------|
| `Arrow Keys`           | Move                             |
| `Right Ctrl`           | Attack                           |
| `/`                    | Parry                            |
| `Enter`                | Dodge                            |
| `Right Shift` (hold)  | Block                            |

## How the netcode works

The **server owns the entire game** - positions, health, stamina,
who's attacking, who parried whom. It ticks at a fixed 60Hz (bumped up
from an earlier 30Hz specifically to cut input latency - see
"Feel/latency" below). The **client never computes gameplay**; it just
sends your input (move direction + button presses) to the server and
renders whatever the server's last `StateUpdate` packet says. This
means there's no client-side prediction of *outcomes* and no
cheating-by-modified-client - if it didn't happen on the server, it
didn't happen. (There is one purely cosmetic exception - see
"Feel/latency" below.)

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

**Confirmed working:** the SFML 3 API calls for texture/sound-buffer
loading and music looping in `Renderer.cpp`/`AudioManager.cpp` were a
best-guess when first written and have since been confirmed correct -
this all compiled and ran.

## Building

This project links SFML **statically** (`x64-windows-static` vcpkg
triplet), so the resulting `client.exe`/`server.exe` have **no
external DLL dependencies** - copy just the `.exe` (plus the
`assets/` folder, which is separate - see "Distributing a build"
below) to another machine with nothing installed and it runs.

### Windows (VS Code + MSVC + vcpkg + CMake Tools)

`.vscode/settings.json` is already pointed at the static triplet and
your Build Tools vcpkg. In VS Code: **CMake: Delete Cache and
Reconfigure**, then **Build**. First build after a fresh clone (or
after any triplet change) will take a few minutes since vcpkg has to
compile SFML and its dependencies from source for that triplet - this
is normal, not a hang.

Command-line equivalent, if you ever need it:
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release
```

Binaries land in `build/Release/` (or `build/` depending on your
generator): `client.exe`, `server.exe`, `headless_test.exe`.

> **This code targets SFML 3.x.** If your vcpkg instead gives you
> SFML 2.x, the APIs differ enough (event handling,
> `sf::Text`/`sf::FloatRect` constructors, scoped
> `Keyboard::Key`/`Mouse::Button` enums, `openFromFile` vs
> `loadFromFile`) that `client/Renderer.cpp`, `client/AudioManager.cpp`
> and `client/main_client.cpp` would need patching back - ping me with
> the compile errors.

### Distributing a build to someone else's PC

Static linking solves the "missing SFML DLL" problem, but it doesn't
embed the sprite/audio files *into* the exe - those still load from
disk at runtime. To hand the game to someone else, zip up and send:

```
client.exe
assets/          <- the whole folder, as-is
```

Both need to stay in the same relative layout (the exe looks for
`assets/` right next to itself, wherever it's placed - see
`common/PathUtils.h` if you're curious how). If you want a *literal*
single file with no companion folder at all, that means embedding the
sprite/audio bytes directly into the binary as resources instead of
loading from disk - doable, but a separate step from what's built
here; say the word if you want that.

### Linux

```bash
sudo apt install libsfml-dev cmake g++
cmake -B build -S .
cmake --build build -j4
```

Binaries land in `build/`: `client`, `server`, `headless_test`. (The
static-triplet setup above is Windows/vcpkg-specific; on Linux this
links against your distro's SFML package normally.)

## Running

**Local Co-op (two players, one PC, one keyboard):** click **Local
Co-op** on the title screen. This starts an embedded server and
connects two local players into it automatically - no IP typing
needed. Player 1 uses WASD/J/K/Space/Shift, Player 2 uses
Arrows/RCtrl/`/`/Enter/RShift (both schemes are shown on-screen during
the match). Both players share the same view since the whole arena
already fits on screen at once.

**Two players on the same machine, two windows (testing netcode):**
run `client`, click **Create Server**, play as player 0. Run a second
`client`, click **Join**, type `127.0.0.1`, play as player 1.

**Two players on different machines:** one player runs `client` and
clicks **Create Server** (make sure UDP port `9422` is open/forwarded
on their network). They'll land on a "Waiting for opponent to join..."
screen showing their LAN IP and port in a big, hard-to-miss card - read
that out (or screenshot it) to the other player. That player runs
`client`, clicks **Join**, and types the IP shown.

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

## Feel / latency

Button presses were feeling delayed, so two things changed:

- **Server tick rate: 30Hz → 60Hz.** The server only looks at input
  once per tick, so this halves the worst-case time a press can sit
  before the server even sees it. Attack windup was also trimmed
  slightly (120ms → 90ms) for a snappier swing without removing the
  telegraph entirely.
- **Optimistic local swing sound.** The attack-swing sound now plays
  the instant *you* press attack, if the last known server state says
  the attack would actually be allowed to start (not on cooldown,
  enough stamina) - rather than waiting for the round-trip
  confirmation. This is purely a client-side sound preview to mask
  network latency; it never decides whether the attack actually
  happens, and if the server ends up not starting the attack for some
  reason, nothing else about that swing occurs - just the sound cue,
  which is a fair trade for how rarely the guess is wrong. The
  authoritative outcome (positions, damage, parries) is exactly as
  server-driven as before.

## What's implemented vs. not yet

**Working:** movement, player-vs-player collision, melee attacks,
blocking, dodging with i-frames, parrying (punishes the attacker with
a stagger instead of landing damage), stamina costs/regen, health,
win/lose detection, sprite-animated characters (walk/attack/parry/
block/stagger/death, using real observed movement direction for the
walk cycle), idle breathing motion, blood-splash hit effects, screen
shake on clean hits, title/battle music, combat and UI sound effects,
title screen, Create/Join flow with a proper "waiting for opponent"
screen showing the host's IP, and Local Co-op (two players, one
keyboard, split keybindings, shared view).

**Not yet built:** reconnect/timeout handling if a player's client
crashes mid-match, matchmaking codes (Join still takes a raw IP), a
dedicated block/parry sprite pose (currently improvised from the
sheet's Thrust/Spellcast rows - see the Assets section), single-file
asset embedding (assets still ship as a separate folder next to the
exe - see "Distributing a build" above).

## Project layout

```
common/    Shared wire protocol, UDP socket wrapper, executable-path
           helper, small math helpers
server/    Authoritative simulation (GameServer) + standalone server.exe entry point
client/    SFML rendering (sprites/animation/HUD), audio (music/SFX),
           input capture, title screen, and networking client
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
