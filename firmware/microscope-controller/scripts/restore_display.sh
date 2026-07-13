#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-}"
IMAGE="${2:-}"
PYTHON="${PYTHON:-python3}"

if [[ -z "$PORT" || -z "$IMAGE" || ! -f "$IMAGE" ]]; then
  echo "Usage: $0 /dev/cu.usbserial-... /path/to/esp-tft35-v4.1-full.bin" >&2
  exit 1
fi

if [[ "$(wc -c < "$IMAGE" | tr -d ' ')" != 4194304 ]]; then
  echo "Refusing restore: expected an exact 4 MB display image." >&2
  exit 1
fi

echo "Restoring display image with SHA-256:"
shasum -a 256 "$IMAGE"
"$PYTHON" -m esptool --chip esp32 --port "$PORT" --baud 115200 \
  write-flash --flash-mode qio --flash-freq 80m --flash-size 4MB 0x0 "$IMAGE"
