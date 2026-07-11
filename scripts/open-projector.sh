#!/usr/bin/env bash
# Projector machine role: show the Pi's raw camera feed and enable 2-way audio.
# If no Pi host is supplied, discover it by UDP broadcast.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-8090}"
PI_HOST="${1:-${PI_HOST:-auto}}"
if [ "$PI_HOST" = "auto" ] || [ -z "$PI_HOST" ]; then
  echo "Discovering Spacehands Pi on the local network..."
  PI_HOST="$(python3 "$ROOT/scripts/discover.py" find 8 || true)"
  if [ -z "$PI_HOST" ]; then
    echo "ERROR: Could not find Spacehands Pi by broadcast."
    echo "Make sure the Pi is running: make"
    echo "Or pass it explicitly: make PI=raspberrypi.local"
    exit 1
  fi
  echo "Found Spacehands Pi: $PI_HOST"
fi
cd "$ROOT/assets"
python3 -m http.server "$PORT" >/tmp/spacehands-projector-http.log 2>&1 &
HTTP_PID=$!
cleanup() { kill "$HTTP_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM
URL="http://localhost:${PORT}/projector.html?pi=${PI_HOST}"
echo "Opening $URL"
if command -v open >/dev/null 2>&1; then
  open "$URL"
elif command -v xdg-open >/dev/null 2>&1; then
  xdg-open "$URL"
else
  echo "Open this URL in a browser: $URL"
fi
wait "$HTTP_PID"
