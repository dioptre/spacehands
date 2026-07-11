#!/usr/bin/env bash
# Stop leftover Spacehands runtime processes from previous foreground runs.
# Does not uninstall the systemd service; use `make uninstall` for that.
set -euo pipefail

SERVICE_NAME="spacehands-pi.service"

if command -v systemctl >/dev/null 2>&1 && systemctl list-unit-files "$SERVICE_NAME" >/dev/null 2>&1; then
  if systemctl is-active --quiet "$SERVICE_NAME" 2>/dev/null; then
    echo "Stopping active $SERVICE_NAME..."
    sudo systemctl stop "$SERVICE_NAME" || true
  fi
fi

# Kill by script/process names. Ignore failures if nothing is running.
pkill -f "scripts/webrtc-audio-pi.py" 2>/dev/null || true
pkill -f "webrtc-audio-pi.py" 2>/dev/null || true
pkill -f "scripts/discover.py announce" 2>/dev/null || true
pkill -f "discover.py announce" 2>/dev/null || true
pkill -f "build/instrument.*--config" 2>/dev/null || true
pkill -f "/instrument.*--config" 2>/dev/null || true
pkill -f "ffmpeg.*-f alsa" 2>/dev/null || true
pkill -f "arecord.*S16_LE" 2>/dev/null || true
pkill -f "aplay.*S16_LE" 2>/dev/null || true

sleep 0.5

# If ports are still held, show diagnostics but do not hard-fail.
for port in 8080 8081 8082 8091; do
  if command -v lsof >/dev/null 2>&1; then
    if lsof -iTCP:"$port" -sTCP:LISTEN >/dev/null 2>&1; then
      echo "WARNING: port $port is still in use:"
      lsof -iTCP:"$port" -sTCP:LISTEN || true
    fi
  elif command -v ss >/dev/null 2>&1; then
    if ss -ltn "sport = :$port" | grep -q ":$port"; then
      echo "WARNING: port $port is still in use:"
      ss -ltnp "sport = :$port" || true
    fi
  fi
done

echo "Spacehands runtime reset complete."
