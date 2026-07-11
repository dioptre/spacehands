#!/usr/bin/env bash
# Raspberry Pi role: read the Arducam ToF camera and publish the raw feed.
# Open http://<pi-ip>:8082/stream.mjpeg from the projector machine.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
CONFIG="${CONFIG:-$ROOT/config.projector-pi.json}"
NCPU="$(nproc 2>/dev/null || echo 4)"
# Give undervolted Pi setups a better chance. Override with BUILD_JOBS=<n>.
BUILD_JOBS="${BUILD_JOBS:-1}"

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 is required for network auto-discovery."
  echo "Install with: sudo apt install python3"
  exit 1
fi

if ! command -v arecord >/dev/null 2>&1 || ! command -v aplay >/dev/null 2>&1; then
  echo "ERROR: ALSA tools are required for WebRTC audio."
  echo "Install with: sudo apt install alsa-utils"
  exit 1
fi

WEBRTC_VENV="$ROOT/.venv-webrtc"
if [ ! -x "$WEBRTC_VENV/bin/python" ]; then
  python3 -m venv "$WEBRTC_VENV"
fi
"$WEBRTC_VENV/bin/python" -m pip install --upgrade pip >/dev/null
"$WEBRTC_VENV/bin/python" -m pip install -r "$ROOT/scripts/webrtc-requirements.txt"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$BUILD_JOBS"

cd "$ROOT"
python3 "$ROOT/scripts/discover.py" announce &
DISCOVER_PID=$!
"$WEBRTC_VENV/bin/python" "$ROOT/scripts/webrtc-audio-pi.py" --port 8091 &
WEBRTC_PID=$!
cleanup() { kill "$DISCOVER_PID" "$WEBRTC_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM
exec "$BUILD/instrument" --config "$CONFIG"
