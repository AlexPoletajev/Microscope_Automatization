#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FLUIDNC_DIR="$(cd "$PROJECT_DIR/../FluidNC" && pwd)"
DATA_DIR="$FLUIDNC_DIR/FluidNC/data"
BUILD_DIR="$PROJECT_DIR/build"
PIO="${PIO:-pio}"

if [[ ! -f "$FLUIDNC_DIR/platformio.ini" ]]; then
  echo "FluidNC submodule missing. Run: git submodule update --init --recursive" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
restore_data() {
  rm -rf "$DATA_DIR"
  mkdir -p "$DATA_DIR"
  cp -a "$tmp_dir/data/." "$DATA_DIR/"
  rm -rf "$tmp_dir"
}
trap restore_data EXIT

mkdir -p "$tmp_dir/data" "$BUILD_DIR"
cp -a "$DATA_DIR/." "$tmp_dir/data/"
rm -rf "$DATA_DIR"
mkdir -p "$DATA_DIR"
cp "$PROJECT_DIR/config.yaml" "$DATA_DIR/config.yaml"
cp -a "$PROJECT_DIR/web/." "$DATA_DIR/"

cd "$FLUIDNC_DIR"
"$PIO" run -e wifi_s3 -t build_merged

cp "$FLUIDNC_DIR/.pio/build/wifi_s3/firmware.bin" "$BUILD_DIR/firmware.bin"
cp "$FLUIDNC_DIR/.pio/build/wifi_s3/littlefs.bin" "$BUILD_DIR/littlefs.bin"
cp "$FLUIDNC_DIR/.pio/build/wifi_s3/merged-flash.bin" "$BUILD_DIR/microscope-stage-merged.bin"

echo "Built $BUILD_DIR/microscope-stage-merged.bin"
