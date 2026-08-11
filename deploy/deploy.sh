#!/usr/bin/env bash
# Build the server statically, run the tests, ship the binary and restart it.
#
#   ./deploy/deploy.sh user@host
#
# Needs cmake and g++ locally, and passwordless sudo on the remote box (or it
# will just prompt). See deploy/README.md for the one-time server setup.

set -euo pipefail

HOST="${1:-${PVPARENA_HOST:-}}"
if [ -z "$HOST" ]; then
    echo "usage: $0 user@host" >&2
    exit 1
fi

REMOTE_DIR=/opt/pvparena
SERVICE=pvparena-server
BUILD_DIR=build-server
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT"

echo "==> building static server"
cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CLIENT=OFF \
    -DSTATIC_SERVER=ON
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "==> running tests"
ctest --test-dir "$BUILD_DIR" --output-on-failure

strip "$BUILD_DIR/server"
echo "==> $(du -h "$BUILD_DIR/server" | cut -f1) static binary, no runtime deps"

echo "==> uploading to $HOST"
scp -q "$BUILD_DIR/server" "$HOST:/tmp/pvparena-server.new"

# You cannot write over a running executable (ETXTBSY), but you can rename on
# top of it - the running process keeps the old inode until it exits.
echo "==> installing and restarting"
ssh "$HOST" "set -e
    sudo install -m 0755 -o root -g root /tmp/pvparena-server.new $REMOTE_DIR/server.new
    sudo mv $REMOTE_DIR/server.new $REMOTE_DIR/server
    rm -f /tmp/pvparena-server.new
    sudo systemctl restart $SERVICE"

sleep 1
ssh "$HOST" "systemctl is-active $SERVICE && sudo journalctl -u $SERVICE -n 5 --no-pager"

echo "==> done"
