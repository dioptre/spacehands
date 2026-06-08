#!/usr/bin/env bash
# Raspberry Pi production launcher.
# Expects the binary to be pre-built at /opt/instrument/instrument.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="${BINARY:-$ROOT/build/instrument}"
CONFIG="${CONFIG:-$ROOT/config.pi.json}"

export DISPLAY=:0

# 1. Start SuperCollider headless
echo "==> Starting SuperCollider..."
sclang "$ROOT/supercollider/boot.scd" &
SC_PID=$!
sleep 5

# 2. Start the instrument binary
echo "==> Starting instrument binary..."
"$BINARY" --config "$CONFIG" &
BIN_PID=$!
sleep 2

# 3. Launch Chromium in kiosk mode
echo "==> Launching Chromium kiosk..."
chromium-browser \
  --kiosk \
  --no-sandbox \
  --disable-infobars \
  --disable-session-crashed-bubble \
  --noerrdialogs \
  --disable-translate \
  --app=http://localhost:8080 \
  &
CHROM_PID=$!

echo "Running. Kill this script to stop everything."
trap "kill $SC_PID $BIN_PID $CHROM_PID 2>/dev/null; exit 0" INT TERM
wait $BIN_PID
