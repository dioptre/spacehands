#!/usr/bin/env bash
# Install build dependencies for both macOS (brew) and Linux/Pi (apt).
# Usage: ./scripts/deps.sh
set -euo pipefail

OS="$(uname -s)"

if [ "$OS" = "Darwin" ]; then
    echo "==> macOS: installing via Homebrew"
    brew update
    brew install cmake pkg-config
    brew install python@3.12
    brew install opencv   # includes DNN module — used for ONNX hand detection
    brew install liblo
    brew install ffmpeg   # ffmpeg/ffplay used for 2-way projector ↔ Pi audio bridge

    # SuperCollider — install .app via cask if sclang not already present
    if [ ! -x "/Applications/SuperCollider.app/Contents/MacOS/sclang" ] && ! command -v sclang &>/dev/null; then
        echo "==> Installing SuperCollider..."
        brew install --cask supercollider
    else
        echo "==> SuperCollider already installed, skipping."
    fi

    echo ""
    echo "==> macOS deps done."

elif [ "$OS" = "Linux" ]; then
    echo "==> Linux/Pi: installing via apt"
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        pkg-config \
        python3 \
        git \
        libopencv-dev \
        liblo-dev \
        libssl-dev \
        ffmpeg \
        alsa-utils \
        pulseaudio-utils \
        supercollider \
        supercollider-server
    # Note: libopencv-dev includes the DNN module which runs ONNX models on CPU
    # No NCNN needed — OpenCV DNN handles YOLOX hand detection on Pi 5
    # ffmpeg/ffplay handles the constant 2-way audio bridge; alsa-utils helps
    # verify/select Pi microphone and earphone devices (arecord -l, aplay -l).

    echo ""
    echo "==> Installing Arducam ToF SDK..."
    ARDUCAM_INSTALL="$(dirname "$0")/../third_party/Arducam_tof_camera/Install_dependencies.sh"
    if [ -f "$ARDUCAM_INSTALL" ]; then
        bash "$ARDUCAM_INSTALL"
    else
        echo "WARNING: Arducam install script not found at $ARDUCAM_INSTALL"
        echo "  Run: third_party/Arducam_tof_camera/Install_dependencies.sh manually"
    fi

    echo ""
    echo "==> Linux/Pi deps done."
else
    echo "Unsupported OS: $OS"
    exit 1
fi
