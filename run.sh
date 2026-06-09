#!/usr/bin/env bash
# All-in-one: install deps → download model → build → launch
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
    pkg-config --exists liblo   2>/dev/null || NEED_DEPS=1
    pkg-config --exists opencv4 2>/dev/null || NEED_DEPS=1
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
if [ ! -f "$ROOT/models/yolox-hand-n-192x320.onnx" ] && [ ! -f "$ROOT/models/hand_yolov8n.onnx" ]; then
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
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -E "error:|liblo|Platform" || true
cmake --build "$BUILD" -j"$NCPU" 2>&1 | tail -3

# ---- 4. Launch ----
echo ""
echo "[4/4] Launching..."

CONFIG="$ROOT/config.json"
SHADER="${SHADER:-wormhole}"
SC_PID=""
CHROM_PID=""
BIN_PID=""

if [ "$OS" = "Darwin" ]; then
    SCLANG="/Applications/SuperCollider.app/Contents/MacOS/sclang"

    # RESET_SC=1 ./run.sh to kill existing SC and start fresh
    if [ "${RESET_SC:-0}" = "1" ]; then
        echo "  Killing SuperCollider for clean restart..."
        pkill -9 -f "sclang"  2>/dev/null || true
        pkill -9 -f "scsynth" 2>/dev/null || true
        sleep 2
    fi

    # SC must run in GUI on Mac — check it's already up
    if lsof -i UDP:57120 >/dev/null 2>&1; then
        echo "  SuperCollider running ✓"
    else
        echo ""
        echo "  ┌──────────────────────────────────────────────────────┐"
        echo "  │  SuperCollider not running.                          │"
        echo "  │  1. Open SuperCollider.app                           │"
        echo "  │  2. Run startup.scd (Cmd+A then Shift+Enter)         │"
        echo "  │  3. Wait for 'SuperDirt: listening' in post window   │"
        echo "  │  4. Re-run: ./run.sh                                 │"
        echo "  └──────────────────────────────────────────────────────┘"
        exit 1
    fi

    # Kill and restart Tidal
    echo "  Killing any running Tidal instances..."
    pkill -f "ghci" 2>/dev/null || true
    sleep 2

    echo "  Starting Tidal with instrument patterns..."
    osascript -e "tell application \"Terminal\"
        activate
        do script \"cd ~/Documents/tidal && ghci -ignore-dot-ghci -ghci-script boot-instrument.ghci\"
    end tell"
    echo "  Tidal starting (ready in ~15s)..."
    sleep 15

    # Start C++ binary
    echo "  Starting instrument binary..."
    sleep 2
    cd "$ROOT"
    "$BUILD/instrument" --config "$CONFIG" &
    BIN_PID=$!
    sleep 3

    echo "  Opening browser..."
    open "http://localhost:8080/?shader=${SHADER}"

else
    # Linux/Pi
    CONFIG="$ROOT/config.pi.json"
    SCLANG=$(command -v sclang || echo "")
    if [ -n "$SCLANG" ]; then
        echo "  Starting SuperCollider headless..."
        "$SCLANG" ~/Documents/tidal/startup.scd > /tmp/sc_instrument.log 2>&1 &
        SC_PID=$!
        sleep 10
        nohup ghci -ghci-script ~/Documents/tidal/boot-instrument.ghci \
            > /tmp/tidal_instrument.log 2>&1 &
        sleep 6
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
echo "  Shaders: wormhole | voronoi | frequency | hand | offering | transformation"
echo ""
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
