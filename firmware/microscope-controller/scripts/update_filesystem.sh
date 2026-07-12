#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE="$PROJECT_DIR/build/littlefs.bin"
PORT="${1:-}"
PYTHON="${PYTHON:-python3}"

if [[ -z "$PORT" ]]; then
  ports="$(find /dev -maxdepth 1 \( -name 'cu.usbserial-*' -o -name 'cu.wchusbserial*' \) -print 2>/dev/null)"
  if [[ "$(printf '%s\n' "$ports" | sed '/^$/d' | wc -l | tr -d ' ')" -ne 1 ]]; then
    echo "Usage: $0 /dev/cu.usbserial-..." >&2
    exit 1
  fi
  PORT="$ports"
fi

if [[ ! -f "$IMAGE" ]] || [[ "$(wc -c < "$IMAGE" | tr -d ' ')" -ne 2031616 ]]; then
  echo "Valid LittleFS image missing. Run scripts/build.sh first." >&2
  exit 1
fi

if ! "$PYTHON" -c 'import esptool' 2>/dev/null; then
  echo "The selected Python has no esptool module. Install requirements.txt or set PYTHON=/path/to/python." >&2
  exit 1
fi

"$PYTHON" -m esptool --chip esp32s3 --port "$PORT" --baud 460800 \
  write-flash --flash-mode dio --flash-freq 80m --flash-size 8MB 0x610000 "$IMAGE"
