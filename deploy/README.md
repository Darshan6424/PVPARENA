# Deploying the dedicated server

The server has no SFML, no graphics and no audio in it, and reads nothing off
disk. Built with `-DSTATIC_SERVER=ON` it is a single ~1MB static binary with no
runtime dependencies at all - not even libstdc++. Deploying is copying one file.

## One-time server setup

On the box, as root or with sudo:

```bash
sudo mkdir -p /opt/pvparena
sudo ufw allow 9422/udp          # or whatever your firewall is
```

Copy the unit file over and enable it:

```bash
scp deploy/pvparena-server.service user@host:/tmp/
ssh user@host 'sudo mv /tmp/pvparena-server.service /etc/systemd/system/ && sudo systemctl daemon-reload && sudo systemctl enable pvparena-server'
```

It won't start until a binary exists at `/opt/pvparena/server`, which the first
deploy puts there.

The unit uses `DynamicUser=yes`, so there's no account to create - systemd
makes a throwaway one per start. It also runs with a read-only filesystem and
only `AF_INET`/`AF_UNIX` allowed, which is all the server ever needs.

## Deploying and redeploying

Same command both times:

```bash
./deploy/deploy.sh user@host
```

That builds statically, runs the tests, refuses to ship if any fail, uploads
the binary, swaps it in and restarts the service. A redeploy is a ~1MB copy and
a sub-second restart.

Set `PVPARENA_HOST=user@host` in your shell to drop the argument.

Requires `cmake` and `g++` locally (`sudo apt install cmake g++`).

## Building on the server instead

If you'd rather not build locally - say you're deploying from Windows - the
server box only needs a compiler and cmake, no graphics packages:

```bash
sudo apt install g++ cmake git
git clone <repo> /opt/pvparena/src && cd /opt/pvparena/src
cmake -B build -S . -DBUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo install -m0755 build/server /opt/pvparena/server.new
sudo mv /opt/pvparena/server.new /opt/pvparena/server
sudo systemctl restart pvparena-server
```

`-DBUILD_CLIENT=OFF` is what keeps SFML out of the picture; without it cmake
looks for SFML 3 and fails before it gets to the server target.

Redeploying is then `git pull && cmake --build build -j` plus the same three
install lines.

## Checking on it

```bash
systemctl status pvparena-server
journalctl -u pvparena-server -f      # connects, disconnects, timeouts
ss -lunp | grep 9422                  # confirm it's listening on UDP
```

The server logs a line whenever a player connects, leaves or times out. Logs
are line-buffered on purpose so they reach journald immediately.

## Notes

- **UDP, not TCP.** Firewall and any cloud security group need `9422/udp`.
  A TCP-only rule silently drops everything.
- **Two players per server process.** `MAX_PLAYERS` is 2. To host more matches,
  run more instances on different ports - copy the unit as
  `pvparena-server@.service` with `ExecStart=/opt/pvparena/server %i` and
  `systemctl enable --now pvparena-server@9423`.
- **Restart drops players.** There's no reconnect yet, so clients fall back to
  the title screen. Deploy between matches.
- **Idle cost is negligible.** The tick loop sleeps 1ms at a time; expect
  low single-digit CPU and a few MB of RSS.
