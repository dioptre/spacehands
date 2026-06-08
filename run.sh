#!/usr/bin/env bash
# All-in-one: install deps → download model → build → launch
# Mac:   Start SuperCollider IDE manually first (runs startup.scd with SuperDirt)
# Linux: Runs standalone sclang headlessly
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
OS="$(uname -s)"

echo "======================================"
echo "  Wormhole Instrument"
echo "======================================"

# ---- 1. Dependencies ----
echo ""
echo "[1/4] Checking dependencies..."
NEED_DEPS=0
if [ "$OS" = "Darwin" ]; then
    brew list opencv &>/dev/null || NEED_DEPS=1
    brew list liblo  &>/dev/null || NEED_DEPS=1
else
    pkg-config --exists liblo    2>/dev/null || NEED_DEPS=1
    pkg-config --exists opencv4  2>/dev/null || NEED_DEPS=1
fi
if [ "$NEED_DEPS" = "1" ]; then
    echo "  Missing deps — running deps.sh..."
    bash "$ROOT/scripts/deps.sh"
else
    echo "  All deps present."
fi

# ---- 2. Model ----
echo ""
echo "[2/4] Checking model..."
if [ ! -f "$ROOT/models/yolox-hand-n-192x320.onnx" ]; then
    echo "  Model not found — downloading..."
    bash "$ROOT/scripts/download_model.sh"
else
    echo "  Model present."
fi

# ---- 3. Build ----
echo ""
echo "[3/4] Building..."
BUILD="$ROOT/build"
NCPU=$([ "$OS" = "Darwin" ] && sysctl -n hw.ncpu || nproc)
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -E "error:|NCNN|liblo|Platform" || true
cmake --build "$BUILD" -j"$NCPU" 2>&1 | tail -3

# ---- 4. Launch ----
echo ""
echo "[4/4] Launching..."

CONFIG="$ROOT/config.json"
SHADER="${SHADER:-wormhole}"
SC_PID=""
CHROM_PID=""

if [ "$OS" = "Darwin" ]; then
    # Mac: expect SuperCollider IDE to already be running with startup.scd
    # Check if SC is up
    if lsof -i UDP:57120 >/dev/null 2>&1; then
        echo "  SuperCollider running on port 57120 ✓"
    else
        echo ""
        echo "  ┌─────────────────────────────────────────────────────┐"
        echo "  │  SuperCollider is not running.                      │"
        echo "  │                                                     │"
        echo "  │  1. Open SuperCollider IDE                          │"
        echo "  │  2. Open and run: ~/Documents/tidal/startup.scd    │"
        echo "  │  3. Re-run this script                              │"
        echo "  │                                                     │"
        echo "  │  Or press Enter to continue without audio.         │"
        echo "  └─────────────────────────────────────────────────────┘"
        read -r _
    fi

    echo "  Starting instrument binary..."
    cd "$ROOT"
    "$BUILD/instrument" --config "$CONFIG" &
    BIN_PID=$!
    sleep 3
    echo "  Opening browser..."
    open "http://localhost:8080/?shader=${SHADER}"

else
    # Linux/Pi: run SC headless, use real camera config
    CONFIG="$ROOT/config.pi.json"
    SCLANG=$(command -v sclang || echo "")
    if [ -n "$SCLANG" ]; then
        echo "  Starting SuperCollider headless..."
        "$SCLANG" "$ROOT/supercollider/boot.scd" > /tmp/sc_instrument.log 2>&1 &
        SC_PID=$!
        sleep 6
    else
        echo "  WARNING: sclang not found — audio disabled."
    fi

    echo "  Starting instrument binary..."
    cd "$ROOT"
    "$BUILD/instrument" --config "$CONFIG" &
    BIN_PID=$!
    sleep 2

    export DISPLAY=:0
    chromium-browser --kiosk --no-sandbox \
        --disable-infobars --noerrdialogs \
        --app="http://localhost:8080/?shader=${SHADER}" &
    CHROM_PID=$!
fi

echo ""
echo "======================================"
echo "  http://localhost:8080/?shader=${SHADER}"
echo "  Shaders: wormhole | voronoi | frequency"
echo "  Press Ctrl+C to stop"
echo "======================================"

cleanup() {
    echo "Shutting down..."
    kill $BIN_PID ${SC_PID:-} ${CHROM_PID:-} 2>/dev/null
    wait 2>/dev/null
    exit 0
}
trap cleanup INT TERM
wait $BIN_PID
