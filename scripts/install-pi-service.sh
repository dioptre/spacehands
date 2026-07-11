#!/usr/bin/env bash
# Install/update the Spacehands Raspberry Pi systemd service.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
USER_NAME="${SUDO_USER:-$(id -un)}"
GROUP_NAME="$(id -gn "$USER_NAME" 2>/dev/null || echo "$USER_NAME")"
SERVICE_NAME="spacehands-pi.service"
SERVICE_PATH="/etc/systemd/system/$SERVICE_NAME"
CONFIG_PATH="${CONFIG:-$ROOT/config.projector-pi.json}"

if [ ! -d /run/systemd/system ]; then
  echo "ERROR: systemd does not appear to be running on this machine."
  exit 1
fi

if [ ! -f "$ROOT/scripts/run-pi-camera-stream.sh" ]; then
  echo "ERROR: missing $ROOT/scripts/run-pi-camera-stream.sh"
  exit 1
fi

sudo tee "$SERVICE_PATH" >/dev/null <<EOF
[Unit]
Description=Spacehands Pi camera stream and two-way audio bridge
After=network-online.target sound.target
Wants=network-online.target

[Service]
Type=simple
User=$USER_NAME
Group=$GROUP_NAME
WorkingDirectory=$ROOT
Environment=HOME=/home/$USER_NAME
Environment=CONFIG=$CONFIG_PATH
ExecStart=$ROOT/scripts/run-pi-camera-stream.sh
Restart=always
RestartSec=3
KillSignal=SIGTERM
TimeoutStopSec=10

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"
sudo systemctl restart "$SERVICE_NAME"

echo "Installed and started $SERVICE_NAME"
echo "Status:  systemctl status $SERVICE_NAME"
echo "Logs:    journalctl -u $SERVICE_NAME -f"
