#!/usr/bin/env bash
# Builds everything this machine can build, into dist/.
#
#   ./scripts/build-all.sh
#
# Produces:
#   dist/server                     dedicated server, statically linked
#   dist/client + dist/assets       only if SFML 3 is installed
#   dist/checksums.txt
#   a local pvparena-server docker image, if docker is available
#
# Windows executables are not built here. Cross-compiling them from Linux
# would mean a whole second SFML toolchain, so they come from CI instead:
# push a tag like v1.0.0 and .github/workflows/release.yml builds and
# publishes both platforms.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR=build
DIST=dist
JOBS="$(nproc 2>/dev/null || echo 4)"

rm -rf "$DIST"
mkdir -p "$DIST"

echo "==> server + tests (no SFML needed)"
cmake -B "$BUILD_DIR/server" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CLIENT=OFF \
    -DSTATIC_SERVER=ON >/dev/null
cmake --build "$BUILD_DIR/server" -j"$JOBS"

echo "==> tests"
ctest --test-dir "$BUILD_DIR/server" --output-on-failure

strip "$BUILD_DIR/server/server"
cp "$BUILD_DIR/server/server" "$DIST/server"
echo "    dist/server  ($(du -h "$DIST/server" | cut -f1), $(file -b "$DIST/server" | grep -o 'statically linked\|dynamically linked'))"

echo "==> client"
if cmake -B "$BUILD_DIR/client" -S . -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1; then
    cmake --build "$BUILD_DIR/client" -j"$JOBS"
    cmake --install "$BUILD_DIR/client" --prefix "$DIST" >/dev/null
    echo "    dist/client + dist/assets"
else
    echo "    skipped: SFML 3 not found."
    echo "    Install it (see README) or let CI build the client."
fi

if command -v docker >/dev/null 2>&1; then
    echo "==> docker image"
    docker build -t pvparena-server:local . >/dev/null 2>&1
    echo "    pvparena-server:local  ($(docker images pvparena-server:local --format '{{.Size}}'))"
else
    echo "==> docker image skipped: docker not installed"
fi

( cd "$DIST" && find . -type f -not -name checksums.txt -exec sha256sum {} + > checksums.txt )

echo
echo "==> done. Contents of $DIST:"
find "$DIST" -maxdepth 1 -mindepth 1 -printf "    %f\n" | sort
