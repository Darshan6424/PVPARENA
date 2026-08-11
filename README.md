# PvP Arena

A 1v1 sword fighting game for two players over a network.

Two fighters meet in a small arena. You can move, attack, block, dodge and
parry. Each attack costs stamina. The first player to reach zero health loses.

The game is written in C++17. It uses SFML 3 for graphics and sound, and plain
UDP for the network. It does not use a game engine.

---

## Words used in this guide

If you are not sure about a word below, read this list first.

| Word | Meaning |
|---|---|
| **Client** | The program the player runs. It shows the game and reads the keyboard. |
| **Server** | The program that runs the rules. It decides who is hit and how much health is lost. |
| **Host** | The computer that runs the server. |
| **LAN** | Local network. All computers in the same house or office, on the same Wi-Fi or cable. |
| **LAN IP** | The address of a computer inside a LAN. It usually starts with `192.168.` or `10.` |
| **Public IP** | The address of your internet connection, seen from outside your house. |
| **Port** | A number that picks one program on a computer. This game uses port `9422`. |
| **UDP** | The type of network message this game uses. It is **not** TCP. This matters for firewalls. |
| **Firewall** | Software that blocks network messages. You must allow port 9422 UDP. |
| **Port forwarding** | A router setting. It sends messages from the internet to one computer in your LAN. |

---

## Controls

**Player 1 (also used when you play alone):**

| Key | Action |
|---|---|
| `W` `A` `S` `D` | Move |
| `J` or left mouse click | Attack |
| `K` or right mouse click | Parry |
| `Space` | Dodge (you cannot be hit for a short time) |
| `Left Shift` (hold down) | Block (less damage, but stamina goes down) |
| `R` | Play again, on the win screen |
| `Esc` | Go back to the title screen |

**Player 2 (only in Local Co-op):**

| Key | Action |
|---|---|
| Arrow keys | Move |
| `Right Ctrl` | Attack |
| `/` | Parry |
| `Enter` | Dodge |
| `Right Shift` (hold down) | Block |

---

## Quick start: play on one computer

You do not need a network for this.

1. Build the client. See [Building the client](#building-the-client).
2. Start the client.
3. Click **Local Co-op**.

Two players now share one keyboard. The game starts a server inside the client
program, so there is nothing else to set up.

---

## Project layout

```
src/
  common/          Message format, UDP socket, Vec2 maths, exe path helper
  server/
    sim/           Fighter and ArenaSim - the rules, no input or output
    net/           Session and GameServer - sockets and the tick loop
    main.cpp       The dedicated server program
  client/
    app/           Game (owns everything) and MatchWatcher
    screens/       Screen base class and the four screens
    net/           GameClient and EmbeddedServer
    view/          Assets, Renderer, AudioManager
    input/         InputCapture
    main.cpp       The client program
tests/             Four test programs. No screen and no SFML needed.
deploy/            systemd service file and a deploy script
assets/            Images, sounds and the font the client loads at run time
```

Every file includes headers from the `src` root, so a header is written the
same way everywhere:

```cpp
#include "server/sim/Fighter.h"
```

Three folders explain most of the design:

- **`server/sim`** holds the rules and nothing else. `Fighter` owns one
  duellist's health, stamina and current action, and is the only code allowed
  to change them. `ArenaSim` is the referee: it decides who is in range of who,
  pushes overlapping fighters apart, and ends the match. Neither one opens a
  socket, starts a thread or reads a clock. That is why the rule tests finish
  in about one millisecond instead of playing real matches in real time.
- **`client/screens`** is a small class hierarchy. `Screen` is an abstract base
  class with `handleEvent`, `update` and `draw`. `TitleScreen`, `AddressScreen`,
  `ConnectingScreen` and `MatchScreen` each override what they need. The main
  loop calls the same three methods every frame and never asks which screen is
  active.
- **`client/view`** holds `Assets`, which is the only class that opens a file.
  A list at the top of `Assets.cpp` maps each name to each path, so no other
  file in the client builds a path string. `Game` owns the one `Assets` object
  and passes it to `Renderer` and `AudioManager` by reference.

---

## How the network works

The server owns the whole game. It decides positions, health, stamina, and who
parried who. It runs 60 times per second. The client only sends your key
presses and draws the last picture the server sent. The client never decides
the result of anything.

Message flow:

1. Client sends `ConnectRequest`. Server answers `ConnectAccept` and gives the
   client a player number, 0 or 1.
2. Client sends `InputState` every frame.
3. Server sends `StateUpdate` to both players every tick.
4. When both players send `Rematch`, the server starts a new match.

Three safety rules are built in:

- The server decides which player sent a message by looking at the **address
  the message came from**, not the player number written inside the message.
  So one player cannot control or disconnect the other player.
- Old messages that arrive late are thrown away.
- A client that sends nothing for 8 seconds loses its slot.

---

## Building the client

The client needs **SFML version 3**. SFML 2 will not work. The two versions
have different function names, so the code will not compile with SFML 2.

The Docker image described later contains **only the server**. You always build
the client separately, on the computer where you want to play.

### Windows (Visual Studio Build Tools + vcpkg + VS Code)

1. Open `.vscode/settings.json`.
2. Change `CMAKE_TOOLCHAIN_FILE` to the path of your own vcpkg installation.
3. In VS Code, run **CMake: Delete Cache and Reconfigure**.
4. Build.

The first build takes a long time. vcpkg compiles SFML from source. This is
normal. It is not frozen.

Do not change `VCPKG_TARGET_TRIPLET`. It must stay `x64-windows-static`,
because `CMakeLists.txt` sets the static C runtime. If the two settings do not
match, the build fails, or the program crashes later because it uses two
separate memory heaps.

Command line version:

```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release
```

### Linux

Most Linux distributions still package SFML 2.6. That version does not work.
Check your version first:

```bash
apt-cache policy libsfml-dev     # Debian or Ubuntu
```

If it shows 3.x, install it:

```bash
sudo apt install libsfml-dev cmake g++
```

If it shows 2.x, build SFML 3 from source instead:

```bash
# Libraries that SFML needs
sudo apt install cmake g++ git \
    libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
    libgl1-mesa-dev libfreetype-dev libopenal-dev libvorbis-dev libflac-dev

git clone --branch 3.1.0 --depth 1 https://github.com/SFML/SFML.git
cmake -B sfml-build -S SFML -DCMAKE_BUILD_TYPE=Release
cmake --build sfml-build -j
sudo cmake --install sfml-build
sudo ldconfig
```

Any 3.x version works. Then build the game:

```bash
cmake -B build -S .
cmake --build build -j
```

### Sending the game to another player

```bash
cmake --install build --prefix dist
```

This creates a `dist` folder with the programs and the `assets` folder next to
them. Send the whole folder. The client reads images, sounds and the font from
`assets` while it runs, so `assets` must always stay next to the client
program.

---

## Automated builds and releases

GitHub Actions does the work. The workflows live in `.github/workflows`.

### On every push and pull request (`ci.yml`)

| Job | What it does |
|---|---|
| **Tests** | Builds with `-DBUILD_CLIENT=OFF` and runs all four test programs. No SFML, so it finishes in seconds. This is the gate. |
| **Static server** | Builds the deployable server and *checks* it is really static. The build fails if it is not. |
| **Client (Linux)** | Builds the full client with SFML 3 from vcpkg. |
| **Client (Windows)** | Same, with the `x64-windows-static` triplet. |
| **Docker image** | Builds the image and starts it, then checks the log says it is listening. |

Building SFML from source is slow, so the built packages are cached. Only the
first run pays for it.

### On a version tag (`release.yml`)

Push a tag and the release builds itself:

```bash
git tag v1.0.0
git push origin v1.0.0
```

The tests run first. Nothing is published if they fail. Then it builds and
attaches:

- `pvparena-windows-x64.zip` - Windows client and server plus `assets`
- `pvparena-linux-x86_64.tar.gz` - Linux client and server plus `assets`
- `pvparena-server-linux-x86_64.tar.gz` - just the dedicated server, static
- `SHA256SUMS.txt` - checksums for all of the above

It also pushes the container image to GitHub's registry, tagged with the
version and with `latest`:

```bash
docker run -d --network host --restart unless-stopped ghcr.io/<owner>/<repo>/server:latest
```

You can also start a release by hand from the Actions tab, which asks for the
tag name.

### Security scanning (`codeql.yml`)

CodeQL scans the C++ on every push, every pull request, and once a week. The
weekly run matters because the rule set changes even when the code does not.

### Dependency updates (`.github/dependabot.yml`)

Dependabot opens weekly pull requests for GitHub Actions versions (grouped into
one PR) and for the Debian base image in the `Dockerfile`.

Dependabot has no vcpkg support, so SFML is **not** updated automatically. It
is pinned by `builtin-baseline` in `vcpkg.json`, and `VCPKG_COMMIT` in the
workflows points at the same commit. To move to a newer SFML, change both
together so CI and your machine stay in agreement.

### Building everything locally

```bash
./scripts/build-all.sh
```

That builds the static server, runs the tests, builds the client if SFML 3 is
installed, builds the Docker image if Docker is installed, and writes
everything to `dist/` with checksums.

It does **not** build the Windows executable. Cross-compiling that from Linux
would need a second SFML toolchain, so Windows builds come from CI.

---

## Running the server

You have three options. Docker is the easiest for a real server.

### Option A: Docker (recommended)

You need Docker with the Compose plugin.

```bash
docker compose up -d --build
```

That is the whole installation. The build runs the tests, so a broken build
never becomes an image.

The image is about 1.2 MB. The server is linked statically, which means the
program contains everything it needs. The final image is built `FROM scratch`,
so it holds one file and nothing else: no Linux distribution, no shell, no
package manager.

Useful commands:

```bash
docker compose ps               # is it running?
docker compose logs -f          # watch players join and leave
docker compose down             # stop and remove it
docker compose up -d --build    # apply new code
```

### Option B: static binary and systemd

Use this if you do not want Docker. Full steps are in
[`deploy/README.md`](deploy/README.md).

Short version:

```bash
./deploy/deploy.sh user@your-server
```

This builds the server, runs the tests, copies one file to your server and
restarts the service.

### Option C: build on the server itself

The server needs no graphics libraries. `-DBUILD_CLIENT=OFF` is the important
part. Without it, CMake looks for SFML and stops with an error.

```bash
sudo apt install g++ cmake git
git clone <your repo> pvparena && cd pvparena
cmake -B build -S . -DBUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/server 9422
```

---

## Production checklist

Check every point below before you let real players connect.

### 1. Open the port for UDP, not TCP

This is the most common mistake. The game only uses UDP. A rule that allows
TCP will let nothing through, and the client will just show
"Connection timed out".

On the server computer:

```bash
sudo ufw allow 9422/udp          # Ubuntu and Debian
sudo firewall-cmd --permanent --add-port=9422/udp && sudo firewall-cmd --reload   # Fedora, RHEL
```

If your server is at a cloud provider (AWS, Google Cloud, Azure, Hetzner,
Oracle and others), there is a **second** firewall in their web console. It is
often called a security group, a network rule or a cloud firewall. You must add
UDP port 9422 there as well. Opening only the Linux firewall is not enough.

Check that the server is listening:

```bash
ss -lun | grep 9422
```

### 2. Choose the right Docker network mode

`docker-compose.yml` uses `network_mode: host`. This means the container shares
the network of the host computer. Use this on Linux. There are two reasons:

- Real player addresses appear in the logs. With Docker's normal bridge
  network, every player can look like the same internal Docker address.
- There is no address translation, so there is slightly less delay.

With host mode, the `ports:` setting does nothing. The host firewall rule is
what opens the port.

If you are not on Linux, or you need to change the port number from outside,
use bridge mode instead. Remove `network_mode: host` and add:

```yaml
    ports:
      - "9422:9422/udp"
```

Do not forget `/udp` at the end.

### 3. Understand the restart behaviour

`restart: unless-stopped` is already set. It was tested:

- If the server program crashes or exits with an error, Docker starts it again.
- If **you** stop it with `docker compose stop` or `docker kill`, Docker treats
  this as intentional and does **not** start it again. This is normal Docker
  behaviour, not a bug.

The server handles `SIGTERM`, so `docker compose stop` finishes in well under
one second instead of waiting for the 10 second timeout.

### 4. Remember: one container holds one match

`MAX_PLAYERS` is 2. One server process serves exactly two players. If a third
player tries to join, the server answers "Server full".

To host several matches at the same time, run several containers on different
ports. Add more services to `docker-compose.yml`:

```yaml
  server2:
    image: pvparena-server
    network_mode: host
    restart: unless-stopped
    command: ["9423"]
```

Open every extra port in the firewall too.

### 5. Know that you cannot open a shell in the container

The image is built `FROM scratch`. It contains no shell, so this will fail:

```bash
docker exec -it pvparena-server sh     # does not work
```

This is intentional. A smaller image means fewer security problems. Read the
logs instead:

```bash
docker compose logs -f
```

If you really need a shell for debugging, change the last section of
`Dockerfile` to:

```dockerfile
FROM debian:bookworm-slim
COPY --from=build /src/build/server /server
USER 65534:65534
ENTRYPOINT ["/server"]
CMD ["9422"]
```

The image becomes about 115 MB instead of 1.2 MB.

### 6. Log rotation is already configured

`docker-compose.yml` keeps at most 3 log files of 10 MB each. Without this
setting, Docker logs grow until the disk is full. If you use systemd instead of
Docker, `journald` already limits log size.

### 7. Restarting disconnects the players

There is no reconnect feature yet. When you update or restart the server, both
players return to the title screen and must join again. Update between matches.

### 8. Understand the security limits

Please read this part carefully before you put the server on the public
internet.

- **There are no accounts and no passwords.** Anybody who knows the address and
  the port can join.
- **Messages are not encrypted.** Somebody who can watch the network can read
  the positions and health values. There are no passwords in the messages, so
  there is nothing secret to steal, but be aware of it.
- **There is no protection against flooding.** Somebody could take both player
  slots and stop real players from joining. A slot becomes free again 8 seconds
  after that client stops sending messages.
- **One player cannot control another player.** The server checks the address
  every message came from. This is tested.

The container helps here. It runs as user `65534`, with a read-only filesystem,
with all Linux capabilities dropped, and with a 128 MB memory limit.

For a private game with friends, this is fine. For a public server, put it
behind a firewall rule that only allows the addresses you trust.

---

## Playing over the internet

### Both players in the same house or office

Use the **LAN IP** of the host. The host sees it on the
"Waiting for opponent" screen. It looks like `192.168.1.42`. The other player
clicks **Join** and types that address.

### Players in different places

1. The host needs their **public IP**. Search the internet for "what is my ip".
2. The host must set up **port forwarding** on their router: forward UDP port
   9422 to the LAN IP of the computer running the server.
3. The other player clicks **Join** and types the public IP.

If the host uses a rented server instead of their home computer, port
forwarding is not needed. Only the firewall rules from the checklist matter.

**CGNAT warning.** Some internet providers, and most mobile hotspots, share one
public IP between many customers. This is called CGNAT. With CGNAT, port
forwarding cannot work, whatever you change in the router. If forwarding does
not work and you have already checked everything else, this is the likely
reason. Use a rented server instead.

### Using a different port

The client accepts `IP:port` as well as `IP`. For example `203.0.113.9:9500`.
Start the server with `./server 9500`, or change the `command:` line in
`docker-compose.yml`.

---

## Solving problems

| Problem | Likely cause |
|---|---|
| "Connection timed out" | Port not open, or opened for TCP instead of UDP. Check the cloud firewall too. |
| Works in the LAN but not from the internet | Port forwarding missing, or CGNAT. |
| "Server full" | Two players are already connected. Wait 8 seconds after they leave, or start a second container. |
| The server stopped and did not restart | You stopped it manually. Use `docker compose up -d`. |
| Window opens but there is no text | The font is missing. `assets` must be next to the client program. |
| Characters are coloured circles | The sprite images are missing from `assets`. |
| CMake error about SFML on the server | You forgot `-DBUILD_CLIENT=OFF`. |
| Client will not compile | You have SFML 2. You need SFML 3. |
| Windows link errors naming symbols like `__std_search_4` | vcpkg built SFML with a newer Visual Studio than the one linking your project. `.vscode/settings.json` pins both to VS2022; delete the CMake cache and reconfigure. |

Useful commands:

```bash
docker compose logs -f            # connects, disconnects, timeouts
ss -lun | grep 9422               # is the server listening?
sudo ufw status                   # is the port open?
```

---

## Tests

Four test programs, no test framework:

- `fighter_test` tests one `Fighter` on its own. These checks exist because
  `Fighter` promises things about its own state - health stays between zero and
  the maximum, the dead stay dead, a swing only counts once - and those
  promises are the reason its data is private.
- `sim_test` tests the rules with two fighters: damage, range, the attack
  window, blocking, parrying, dodge invulnerability, stamina, buffered key
  presses, death, restart, collision and arena limits.
- `network_test` starts a real server and uses real UDP, but it drives the
  server by hand, so there are no threads and no waiting. It tests the
  handshake, the "server full" answer, fake messages from strangers, old
  messages, timeouts, reconnecting and rematches.
- `watcher_test` tests `MatchWatcher`, which turns server updates into sound
  and effect events.

Run them all:

```bash
ctest --test-dir build --output-on-failure
```

All four finish in well under one second.

---

## Changing how the game feels

Damage, stamina costs, the parry window, dodge invulnerability time, movement
speed and all other numbers are constants at the top of `src/common/Protocol.h`.
Change a number, rebuild, and the server uses it. The client has no copy of the
rules, so there is nothing else to change.

---

## Assets

`assets/sprites` uses the Universal LPC Spritesheet layout: 64x64 tiles, 13
columns and 21 rows. Player 1 uses the character sheet and player 2 uses the
skeleton sheet, so the two fighters are easy to tell apart. The sheet has no
guard pose, so blocking and parrying reuse the spellcast and thrust poses.

`floor_texture.png` and `floor_decor.png` are the arena ground: a painted dirt
layer with a scattered pebble layer over it, scaled down to the arena size.
They replaced a flat grey rectangle. `title_bg.png` is the backdrop for the
menu screens, drawn dimmed with a dark shade over it so the buttons and text
stay readable.

Only the plain dirt layer of that background pack is used for the floor. The
rest of the pack is painted for a side view, with sky at the top and a horizon
line. This game looks straight down, so those layers would show sky and a
castle wall from overhead. The fully composited scene has no such problem on a
menu screen, because that is a picture rather than a play area, so it is used
there instead.

If any of these files are missing the game still runs. The floor falls back to
a flat colour and the menus to a plain background.

`assets/fonts/DejaVuSans-Bold.ttf` is included with the game. Earlier the game
looked for a font already installed on the computer, and all text disappeared
on computers that did not have one. The licence is next to it in
`DejaVu-LICENSE.txt`.

The sprites and audio came from an older asset pack. Read
`assets/ATTRIBUTION.md` before you share a build.

---

## Not finished yet

- No smoothing between server updates, so movement can look slightly jumpy on a
  poor connection. See the note in `src/client/net/GameClient.h`.
- No reconnect. If the server restarts, players go back to the title screen.
- Join needs a real address. There are no short room codes.
- The assets are a separate folder. They are not built into the program file.
