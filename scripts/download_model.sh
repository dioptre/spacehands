#!/usr/bin/env bash
# Download a pretrained hand detection model in NCNN format.
#
# Options (set MODEL env var):
#   yolov8n-hand    — YOLOv8n fine-tuned on hand detection (default)
#                     Source: community NCNN export, Pi-tested
#   export-custom   — Export your own .pt model to NCNN via Ultralytics
#
# Usage:
#   ./scripts/download_model.sh
#   MODEL=export-custom PT_MODEL=/path/to/best.pt ./scripts/download_model.sh
#
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODELS_DIR="$ROOT/models"
mkdir -p "$MODELS_DIR"

MODEL="${MODEL:-yolox-hand}"

case "$MODEL" in

  yolox-hand)
    echo "==> Downloading YOLOX-nano hand detection model (ONNX, 192x320)..."
    # PINTO model zoo — YOLOX-nano body/head/hand detector
    # Class 2 = hand. CPU-optimised, 3.5MB, ~30ms on Pi 5
    ARCHIVE_URL="https://s3.ap-northeast-2.wasabisys.com/pinto-model-zoo/426_YOLOX-Body-Head-Hand/resources_n.tar.gz"
    TMP=$(mktemp -d)
    curl -sL "$ARCHIVE_URL" -o "$TMP/model.tar.gz"
    tar -xzf "$TMP/model.tar.gz" -C "$TMP" "yolox_n_body_head_hand_0461_0.4428_1x3x192x320.onnx"
    cp "$TMP/yolox_n_body_head_hand_0461_0.4428_1x3x192x320.onnx" \
       "$MODELS_DIR/yolox-hand-n-192x320.onnx"
    rm -rf "$TMP"
    echo "Downloaded to models/yolox-hand-n-192x320.onnx"
    echo ""
    echo "NOTE: Uses OpenCV DNN backend — no NCNN required."
    echo "  Input:   192x320, 3-channel float [0,1]"
    echo "  Classes: 0=body 1=head 2=hand"
    echo "  Format:  [cx, cy, w, h, obj_conf, cls0, cls1, cls2] per anchor"
    ;;

  export-custom)
    # Export a custom .pt model to NCNN via Ultralytics
    PT="${PT_MODEL:-}"
    if [ -z "$PT" ]; then
      echo "ERROR: set PT_MODEL=/path/to/best.pt"
      exit 1
    fi
    echo "==> Exporting $PT to NCNN (imgsz=320 for Pi performance)..."
    python3 -c "
from ultralytics import YOLO
model = YOLO('$PT')
model.export(format='ncnn', imgsz=320)
print('Export complete')
"
    # Ultralytics exports to <name>_ncnn_model/ directory
    NAME=$(basename "$PT" .pt)
    NCNN_DIR="${PT%/*}/${NAME}_ncnn_model"
    if [ -d "$NCNN_DIR" ]; then
      cp "$NCNN_DIR"/*.param "$MODELS_DIR/yolov8n-hand-int8.param"
      cp "$NCNN_DIR"/*.bin   "$MODELS_DIR/yolov8n-hand-int8.bin"
      echo "Copied to models/yolov8n-hand-int8.{param,bin}"
      echo ""
      echo "IMPORTANT: Check layer names in the .param file:"
      echo "  grep '^Input' models/yolov8n-hand-int8.param"
      echo "  grep '^Output' models/yolov8n-hand-int8.param"
      echo "  Update HandDetector.cpp if they differ from in0/out0"
    else
      echo "Export dir not found at $NCNN_DIR — check Ultralytics output"
    fi
    ;;

  *)
    echo "Unknown model: $MODEL"
    echo "Options: yolov8n-hand, export-custom"
    exit 1
    ;;
esac

echo ""
echo "==> Verifying model files..."
for f in "$MODELS_DIR/yolov8n-hand-int8.param" "$MODELS_DIR/yolov8n-hand-int8.bin"; do
  if [ -f "$f" ]; then
    echo "  OK: $f ($(du -sh "$f" | cut -f1))"
  else
    echo "  MISSING: $f"
  fi
done
