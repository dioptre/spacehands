#!/usr/bin/env bash
# Mac development launcher.
# Starts SuperCollider headless, builds and runs the binary, opens the browser.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"

# 1. Build
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5
cmake --build "$BUILD" -j"$(sysctl -n hw.ncpu)" 2>&1 | tail -10

# 2. Start SuperCollider headless — find sclang in .app or PATH
SCLANG=$(which sclang 2>/dev/null || echo "/Applications/SuperCollider.app/Contents/MacOS/sclang")
if [ ! -x "$SCLANG" ]; then
    echo "ERROR: sclang not found. Install SuperCollider from https://supercollider.github.io"
    exit 1
fi
echo "==> Starting SuperCollider ($SCLANG)..."
"$SCLANG" "$ROOT/supercollider/boot.scd" &
SC_PID=$!
sleep 3  # give scsynth time to boot

# 3. Start the instrument binary
echo "==> Starting instrument binary..."
cd "$BUILD"
./instrument --config "$ROOT/config.json" &
BIN_PID=$!
sleep 1

# 4. Open browser
echo "==> Opening http://localhost:8080"
open "http://localhost:8080"

echo ""
echo "Running. Press Ctrl+C to stop."
trap "kill $SC_PID $BIN_PID 2>/dev/null; exit 0" INT TERM
wait $BIN_PID
