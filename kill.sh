#!/usr/bin/env bash
# Kill all running processes related to the Wormhole Instrument
set -euo pipefail

echo "Stopping Wormhole Instrument processes..."

pkill -f sclang || true
pkill -f scsynth || true
pkill -f supernova || true
pkill -f ghci || true
pkill -f instrument || true
pkill -f chromium || true

echo "Cleanup complete."
