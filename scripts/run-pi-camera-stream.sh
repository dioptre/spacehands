#!/usr/bin/env bash
# Raspberry Pi role: read the Arducam ToF camera and publish the raw feed.
# Open http://<pi-ip>:8082/stream.mjpeg from the projector machine.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
CONFIG="${CONFIG:-$ROOT/config.projector-pi.json}"
NCPU="$(nproc 2>/dev/null || echo 4)"

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 is required for network auto-discovery."
  echo "Install with: sudo apt install python3"
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1 || ! command -v ffplay >/dev/null 2>&1; then
  echo "ERROR: ffmpeg/ffplay are required for constant 2-way audio."
  echo "Install with: sudo apt install ffmpeg alsa-utils"
  exit 1
fi

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$NCPU"

cd "$ROOT"
python3 "$ROOT/scripts/discover.py" announce &
DISCOVER_PID=$!
cleanup() { kill "$DISCOVER_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM
exec "$BUILD/instrument" --config "$CONFIG"
