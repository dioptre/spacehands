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
    brew install glfw glew glm

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
        python3-venv \
        python3-pip \
        git \
        libopencv-dev \
        liblo-dev \
        libssl-dev \
        ffmpeg \
        alsa-utils \
        pulseaudio-utils \
        bluez \
        ghc \
        cabal-install \
        libjack-jackd2-dev \
        libsndfile1-dev \
        libfftw3-dev \
        libxt-dev \
        libavahi-client-dev \
        libudev-dev \
        libasound2-dev \
        libreadline-dev \
        libxkbcommon-dev \
        jackd2 \
        pipewire-jack \
        libglfw3-dev \
        libglew-dev \
        libglm-dev \
        sc3-plugins

    # Note: libopencv-dev includes the DNN module which runs ONNX models on CPU.
    # ffmpeg/ffplay handles the constant 2-way audio bridge; alsa-utils helps
    # verify/select Pi microphone and earphone devices (arecord -l, aplay -l).

    # Check if SuperCollider is installed, otherwise build headless from source.
    if command -v sclang &>/dev/null; then
        echo "==> SuperCollider is already installed, skipping compilation."
    else
        echo "==> SuperCollider not found. Building headless SuperCollider from source..."
        (
            SC_TMP="$(mktemp -d)"
            git clone --branch main --recurse-submodules https://github.com/supercollider/supercollider.git "$SC_TMP/supercollider"
            mkdir -p "$SC_TMP/supercollider/build"
            cd "$SC_TMP/supercollider/build"
            cmake -DCMAKE_BUILD_TYPE=Release -DSUPERNOVA=ON -DSC_EL=OFF -DSC_VIM=ON -DNATIVE=ON -DSC_IDE=OFF -DNO_X11=ON -DSC_QT=OFF ..
            make -j$(nproc)
            sudo make install
            sudo ldconfig
            rm -rf "$SC_TMP"
        )
        echo "==> SuperCollider build and install complete."
    fi

    # Build and install mi-UGens if not already present in the Extensions folder.
    EXT_DIR="$HOME/.local/share/SuperCollider/Extensions"
    if [ -d "$EXT_DIR/mi-UGens-aarch64" ]; then
        echo "==> mi-UGens (aarch64) are already installed, skipping compilation."
    else
        echo "==> mi-UGens (aarch64) not found. Compiling from source..."
        (
            UGENS_TMP="$(mktemp -d)"
            git clone --recursive https://github.com/v7b1/mi-UGens.git "$UGENS_TMP/mi-UGens"
            mkdir -p "$UGENS_TMP/mi-UGens/build"
            cd "$UGENS_TMP/mi-UGens/build"

            SC_PATH_VAL="$HOME/supercollider"
            if [ ! -d "$SC_PATH_VAL" ]; then
                echo "    SuperCollider source not found in $HOME/supercollider, cloning shallow copy for headers..."
                SC_PATH_VAL="$UGENS_TMP/sc-headers"
                git clone --depth 1 https://github.com/supercollider/supercollider.git "$SC_PATH_VAL"
            fi

            cmake -DSC_PATH="$SC_PATH_VAL" ..
            make samplerate
            make -j$(nproc)

            # Install to SuperCollider Extensions folder.
            mkdir -p "$EXT_DIR/mi-UGens-aarch64"
            cp -r ../sc/Classes "$EXT_DIR/mi-UGens-aarch64/"
            cp projects/*/*.so "$EXT_DIR/mi-UGens-aarch64/"

            # Also copy to tidal workspace directory for backup/standalone completeness.
            mkdir -p "$HOME/Documents/tidal/mi-UGens"
            cp projects/*/*.so "$HOME/Documents/tidal/mi-UGens/"

            rm -rf "$UGENS_TMP"
        )
        echo "==> mi-UGens compilation and installation complete."
    fi

    if ! ghc -e "import Sound.Tidal.Context" &>/dev/null; then
        echo "==> Installing TidalCycles Haskell library..."
        cabal update
        cabal install tidal --lib
    fi

    echo ""
    ARDUCAM_INSTALL="$(dirname "$0")/../third_party/Arducam_tof_camera/Install_dependencies.sh"
    if [ -f "$ARDUCAM_INSTALL" ]; then
        echo "n" | bash "$ARDUCAM_INSTALL" || true
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
