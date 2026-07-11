#!/usr/bin/env bash
# Stop/disable/remove the Spacehands Raspberry Pi systemd service.
set -euo pipefail

SERVICE_NAME="spacehands-pi.service"
SERVICE_PATH="/etc/systemd/system/$SERVICE_NAME"

if [ ! -d /run/systemd/system ]; then
  echo "ERROR: systemd does not appear to be running on this machine."
  exit 1
fi

sudo systemctl stop "$SERVICE_NAME" 2>/dev/null || true
sudo systemctl disable "$SERVICE_NAME" 2>/dev/null || true
sudo rm -f "$SERVICE_PATH"
sudo systemctl daemon-reload
sudo systemctl reset-failed "$SERVICE_NAME" 2>/dev/null || true

echo "Removed $SERVICE_NAME"
