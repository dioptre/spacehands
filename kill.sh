#!/usr/bin/env bash
# Kill all running processes related to the Wormhole Instrument
set -euo pipefail

echo "Stopping Wormhole Instrument processes..."

pkill -9 -f sclang || true
pkill -9 -f scsynth || true
pkill -9 -f supernova || true
pkill -9 -f ghci || true
pkill -9 -f instrument || true
pkill -9 -f chromium || true

echo "Cleanup complete."
