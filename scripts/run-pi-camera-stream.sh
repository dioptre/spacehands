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
BT_RECONNECT_INTERVAL="${BT_RECONNECT_INTERVAL:-10}"
BT_LAST_RECONNECT=0

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

DISCOVER_PID=""
WEBRTC_PID=""
INSTRUMENT_PID=""
STOPPING=0

bt_mac_from_device() {
  local dev="$1"
  if [[ "$dev" =~ ([0-9A-Fa-f]{2}[:_][0-9A-Fa-f]{2}[:_][0-9A-Fa-f]{2}[:_][0-9A-Fa-f]{2}[:_][0-9A-Fa-f]{2}[:_][0-9A-Fa-f]{2}) ]]; then
    echo "${BASH_REMATCH[1]//_/:}"
  fi
}

pulse_source_exists() {
  local source="$1"
  [ -z "$source" ] && return 0
  [[ "$source" != bluez_* && "$source" != pulse:bluez_* ]] && return 0
  local clean="${source#pulse:}"
  pactl list short sources 2>/dev/null | awk '{print $2}' | grep -Fxq "$clean"
}

pulse_sink_exists() {
  local sink="$1"
  [ -z "$sink" ] && return 0
  [[ "$sink" != bluez_* && "$sink" != pulse:bluez_* ]] && return 0
  local clean="${sink#pulse:}"
  pactl list short sinks 2>/dev/null | awk '{print $2}' | grep -Fxq "$clean"
}

bt_connected() {
  local mac="$1"
  [ -z "$mac" ] && return 0
  bluetoothctl info "$mac" 2>/dev/null | grep -q "Connected: yes"
}

bt_connect_and_profile() {
  local mac="$1"
  [ -z "$mac" ] && return 0
  if ! command -v bluetoothctl >/dev/null 2>&1; then
    echo "[runner] bluetoothctl unavailable; cannot reconnect $mac"
    return 1
  fi

  echo "[runner] attempting Bluetooth reconnect/profile for $mac"
  bluetoothctl power on >/dev/null 2>&1 || true
  bluetoothctl connect "$mac" >/dev/null 2>&1 || true
  sleep 2

  # Try to force headset/handsfree profile so the mic source appears. Different
  # Pulse/PipeWire versions expose different profile names.
  if command -v pactl >/dev/null 2>&1; then
    local card="bluez_card.${mac//:/_}"
    pactl set-card-profile "$card" headset-head-unit >/dev/null 2>&1 || \
    pactl set-card-profile "$card" handsfree-head-unit >/dev/null 2>&1 || \
    pactl set-card-profile "$card" headset-head-unit-cvsd >/dev/null 2>&1 || \
    pactl set-card-profile "$card" headset-head-unit-msbc >/dev/null 2>&1 || true
  fi
}

ensure_audio_devices() {
  local in_dev="${PI_AUDIO_IN:-default}"
  local out_dev="${PI_AUDIO_OUT:-default}"
  local in_mac out_mac now
  in_mac="$(bt_mac_from_device "$in_dev")"
  out_mac="$(bt_mac_from_device "$out_dev")"

  if [ -n "$in_mac" ] && { ! bt_connected "$in_mac" || ! pulse_source_exists "$in_dev"; }; then
    now="$(date +%s)"
    if (( now - BT_LAST_RECONNECT >= BT_RECONNECT_INTERVAL )); then
      BT_LAST_RECONNECT="$now"
      echo "[runner] Bluetooth input missing/not connected: $in_dev"
      bt_connect_and_profile "$in_mac"
    fi
    return 1
  fi

  if [ -n "$out_mac" ] && { ! bt_connected "$out_mac" || ! pulse_sink_exists "$out_dev"; }; then
    now="$(date +%s)"
    if (( now - BT_LAST_RECONNECT >= BT_RECONNECT_INTERVAL )); then
      BT_LAST_RECONNECT="$now"
      echo "[runner] Bluetooth output missing/not connected: $out_dev"
      bt_connect_and_profile "$out_mac"
    fi
    return 1
  fi

  return 0
}

start_discover() {
  python3 "$ROOT/scripts/discover.py" announce &
  DISCOVER_PID=$!
  echo "[runner] discovery started pid=$DISCOVER_PID"
}

start_webrtc() {
  if ! ensure_audio_devices; then
    echo "[runner] audio devices not ready; delaying WebRTC start"
    WEBRTC_PID=""
    return 1
  fi
  "$WEBRTC_VENV/bin/python" "$ROOT/scripts/webrtc-audio-pi.py" \
    --port 8091 \
    --input-device "${PI_AUDIO_IN:-default}" \
    --output-device "${PI_AUDIO_OUT:-default}" &
  WEBRTC_PID=$!
  echo "[runner] WebRTC audio started pid=$WEBRTC_PID input=${PI_AUDIO_IN:-default} output=${PI_AUDIO_OUT:-default}"
}

restart_webrtc() {
  if [ -n "${WEBRTC_PID:-}" ]; then
    kill "$WEBRTC_PID" 2>/dev/null || true
    wait "$WEBRTC_PID" 2>/dev/null || true
  fi
  WEBRTC_PID=""
  start_webrtc || true
}

start_instrument() {
  "$BUILD/instrument" --config "$CONFIG" &
  INSTRUMENT_PID=$!
  echo "[runner] instrument started pid=$INSTRUMENT_PID config=$CONFIG"
}

cleanup() {
  STOPPING=1
  echo "[runner] stopping..."
  kill ${INSTRUMENT_PID:-} ${WEBRTC_PID:-} ${DISCOVER_PID:-} 2>/dev/null || true
  wait ${INSTRUMENT_PID:-} ${WEBRTC_PID:-} ${DISCOVER_PID:-} 2>/dev/null || true
}
trap cleanup EXIT INT TERM

start_discover
start_webrtc || true
start_instrument

# Auto-heal sidecars. If the main C++ instrument exits, let this script exit so
# systemd restarts the entire stack cleanly.
while [ "$STOPPING" = "0" ]; do
  if ! kill -0 "$INSTRUMENT_PID" 2>/dev/null; then
    echo "[runner] instrument exited; exiting so systemd can restart the stack"
    wait "$INSTRUMENT_PID" 2>/dev/null || true
    exit 1
  fi

  if ! kill -0 "$DISCOVER_PID" 2>/dev/null; then
    echo "[runner] discovery died; restarting"
    start_discover
  fi

  if ! ensure_audio_devices; then
    if [ -n "${WEBRTC_PID:-}" ]; then
      echo "[runner] audio devices disappeared; restarting WebRTC when available"
      kill "$WEBRTC_PID" 2>/dev/null || true
      wait "$WEBRTC_PID" 2>/dev/null || true
      WEBRTC_PID=""
    fi
  elif [ -z "${WEBRTC_PID:-}" ] || ! kill -0 "$WEBRTC_PID" 2>/dev/null; then
    echo "[runner] WebRTC audio not running; restarting"
    restart_webrtc
  fi

  sleep 2
done
