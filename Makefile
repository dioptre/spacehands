# Simple role launcher for Spacehands.
#
# Common use:
#   On Raspberry Pi:     make
#   On projector laptop: make
#
# Explicit roles:
#   make pi
#   make projector              # auto-discovers Pi by UDP broadcast
#   make projector PI=192.168.1.23
#   make instrument    # original local instrument launcher

SHELL := /usr/bin/env bash
.DEFAULT_GOAL := run
ROOT := $(CURDIR)
OS := $(shell uname -s)
ARCH := $(shell uname -m)
IS_RPI := $(shell if [ -r /proc/device-tree/model ] && tr -d '\0' < /proc/device-tree/model 2>/dev/null | grep -qi 'Raspberry Pi'; then echo 1; elif [ -r /proc/cpuinfo ] && grep -qi 'Raspberry Pi' /proc/cpuinfo 2>/dev/null; then echo 1; else echo 0; fi)
PI ?= auto
BUILD ?= $(ROOT)/build
CONFIG ?=

.PHONY: all help detect deps check-deps build reset run install-service uninstall service-status service-logs pi projector instrument clean

help:
	@echo "Spacehands Makefile"
	@echo ""
	@echo "Role commands:"
	@echo "  make                            Detect role, install deps if needed, run"
	@echo "                                  Projector laptops auto-discover Pi by UDP broadcast"
	@echo "  make PI=raspberrypi.local       Same, but with explicit Pi host"
	@echo "  make run PI=raspberrypi.local   Same as make"
	@echo "  make pi                         Reset then run Raspberry Pi camera/audio bridge"
	@echo "  make reset                      Stop leftover Pi runtime processes"
	@echo "  make projector PI=<pi-host>     Run projector laptop page"
	@echo "  make uninstall                  On Pi: remove boot service"
	@echo "  make service-status             On Pi: show boot service status"
	@echo "  make service-logs               On Pi: follow boot service logs"
	@echo "  make instrument                 Run original local instrument app"
	@echo ""
	@echo "Setup/build:"
	@echo "  make deps                       Install dependencies"
	@echo "  make check-deps                 Install dependencies only if missing"
	@echo "  make build                      Build C++ app"
	@echo "  make clean                      Remove build directories"
	@echo ""
	@echo "Detected: OS=$(OS) ARCH=$(ARCH) IS_RPI=$(IS_RPI)"

all: run

# Show what this machine was detected as.
detect:
	@echo "OS=$(OS)"
	@echo "ARCH=$(ARCH)"
	@echo "IS_RPI=$(IS_RPI)"
	@if [ "$(IS_RPI)" = "1" ]; then \
	  echo "Role=pi"; \
	else \
	  echo "Role=projector"; \
	fi

# Install everything needed for the current platform.
deps:
	@bash "$(ROOT)/scripts/deps.sh"

# Lightweight dependency check; falls back to scripts/deps.sh if anything important is missing.
check-deps:
	@set -euo pipefail; \
	NEED=0; \
	if [ "$(OS)" = "Darwin" ]; then \
	  command -v brew >/dev/null 2>&1 || { echo "Homebrew is required: https://brew.sh"; exit 1; }; \
	  brew list cmake >/dev/null 2>&1 || NEED=1; \
	  brew list pkg-config >/dev/null 2>&1 || NEED=1; \
	  brew list opencv >/dev/null 2>&1 || NEED=1; \
	  brew list liblo >/dev/null 2>&1 || NEED=1; \
	  brew list ffmpeg >/dev/null 2>&1 || NEED=1; \
	  command -v python3 >/dev/null 2>&1 || NEED=1; \
	elif [ "$(OS)" = "Linux" ]; then \
	  command -v cmake >/dev/null 2>&1 || NEED=1; \
	  command -v pkg-config >/dev/null 2>&1 || NEED=1; \
	  pkg-config --exists opencv4 2>/dev/null || NEED=1; \
	  pkg-config --exists liblo 2>/dev/null || NEED=1; \
	  command -v ffmpeg >/dev/null 2>&1 || NEED=1; \
	  command -v ffplay >/dev/null 2>&1 || NEED=1; \
	  command -v arecord >/dev/null 2>&1 || NEED=1; \
	  command -v aplay >/dev/null 2>&1 || NEED=1; \
	  command -v python3 >/dev/null 2>&1 || NEED=1; \
	else \
	  echo "Unsupported OS: $(OS)"; exit 1; \
	fi; \
	if [ "$$NEED" = "1" ]; then \
	  echo "Missing dependencies; installing..."; \
	  bash "$(ROOT)/scripts/deps.sh"; \
	else \
	  echo "Dependencies OK."; \
	fi

build: check-deps
	@set -euo pipefail; \
	if [ -n "$${BUILD_JOBS:-}" ]; then \
	  NCPU="$${BUILD_JOBS}"; \
	elif [ "$(IS_RPI)" = "1" ]; then \
	  NCPU=1; \
	elif [ "$(OS)" = "Darwin" ]; then \
	  NCPU=$$(sysctl -n hw.ncpu); \
	else \
	  NCPU=$$(nproc); \
	fi; \
	echo "Building with -j$$NCPU"; \
	cmake -S "$(ROOT)" -B "$(BUILD)" -DCMAKE_BUILD_TYPE=Release; \
	cmake --build "$(BUILD)" -j"$$NCPU"

# Auto role:
#   Raspberry Pi: run in foreground for now.
#   Service auto-install is intentionally disabled while we stabilize Pi power/audio.
#   Later, switch the Pi branch back to: $(MAKE) install-service
#   macOS or other Linux laptop: projector page connecting to PI=<host>
run:
	@set -euo pipefail; \
	if [ "$(IS_RPI)" = "1" ]; then \
	  echo "Auto-detected Raspberry Pi; running Pi role in foreground."; \
	  $(MAKE) pi; \
	elif [ "$(OS)" = "Darwin" ] || [ "$(OS)" = "Linux" ]; then \
	  echo "Auto-detected projector/laptop; running projector role."; \
	  $(MAKE) projector PI="$(PI)"; \
	else \
	  echo "Unsupported OS: $(OS)"; exit 1; \
	fi

reset:
	@if [ "$(IS_RPI)" = "1" ] || [ "$(OS)" = "Linux" ]; then \
	  bash "$(ROOT)/scripts/reset-pi-processes.sh"; \
	else \
	  echo "Reset is only needed for Pi/Linux runtime processes."; \
	fi

# Raspberry Pi service role: install/update service, enable it for boot, and start now.
# Not called by default right now; run `make install-service` manually when ready.
install-service: build
	@if [ "$(IS_RPI)" != "1" ]; then \
	  echo "ERROR: install-service is only intended for Raspberry Pi."; \
	  exit 1; \
	fi
	@CONFIG="$(if $(CONFIG),$(CONFIG),$(ROOT)/config.projector-pi.json)" \
	  bash "$(ROOT)/scripts/install-pi-service.sh"

# Raspberry Pi role: camera stream, HTTP endpoints, Pi audio input/output bridge in foreground.
pi: build reset
	@CONFIG="$(if $(CONFIG),$(CONFIG),$(ROOT)/config.projector-pi.json)" \
	  bash "$(ROOT)/scripts/run-pi-camera-stream.sh"

# Remove the Pi boot service. On non-Pi machines this is a no-op with a message.
uninstall:
	@if [ "$(IS_RPI)" = "1" ]; then \
	  bash "$(ROOT)/scripts/uninstall-pi-service.sh"; \
	else \
	  echo "No Spacehands Pi service to uninstall on this machine."; \
	fi

service-status:
	@systemctl status spacehands-pi.service

service-logs:
	@journalctl -u spacehands-pi.service -f

# Projector laptop role: browser page that displays Pi camera and starts 2-way audio.
projector: check-deps
	@bash "$(ROOT)/scripts/open-projector.sh" "$(PI)"

# Original all-in-one/local launcher retained for development/performance mode.
instrument: check-deps
	@bash "$(ROOT)/run.sh"

clean:
	@rm -rf "$(ROOT)/build" "$(ROOT)/build-projector-test"
	@echo "Cleaned build directories."
