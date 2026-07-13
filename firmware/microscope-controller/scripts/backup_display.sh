#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT="${1:-}"
PYTHON="${PYTHON:-python3}"
CHUNK_SIZE=$((0x10000))
CHUNK_COUNT=64

if [[ -z "$PORT" ]]; then
  echo "Usage: $0 /dev/cu.usbserial-..." >&2
  exit 1
fi

stamp="$(date +%Y%m%d-%H%M%S)"
backup_dir="$PROJECT_DIR/backups/display-$stamp"
chunks_dir="$backup_dir/chunks"
image="$backup_dir/esp-tft35-v4.1-full.bin"
mkdir -p "$chunks_dir"

for ((i = 0; i < CHUNK_COUNT; i++)); do
  offset=$((i * CHUNK_SIZE))
  chunk="$(printf '%s/%02d.bin' "$chunks_dir" "$i")"
  success=0

  for attempt in 1 2 3 4 5; do
    if "$PYTHON" -m esptool --chip esp32 --port "$PORT" --baud 230400 \
      read-flash "$offset" "$CHUNK_SIZE" "$chunk" && \
      [[ "$(wc -c < "$chunk" | tr -d ' ')" == "$CHUNK_SIZE" ]]; then
      success=1
      break
    fi
    sleep 1
  done

  if [[ "$success" -ne 1 ]]; then
    echo "Display backup failed at chunk $i after five attempts." >&2
    exit 1
  fi
  printf 'Chunk %02d/%02d verified\n' "$i" "$((CHUNK_COUNT - 1))"
done

dd if=/dev/zero of="$image" bs="$CHUNK_SIZE" count="$CHUNK_COUNT" status=none
for ((i = 0; i < CHUNK_COUNT; i++)); do
  chunk="$(printf '%s/%02d.bin' "$chunks_dir" "$i")"
  dd if="$chunk" of="$image" bs="$CHUNK_SIZE" seek="$i" conv=notrunc status=none
done

[[ "$(wc -c < "$image" | tr -d ' ')" == 4194304 ]]
shasum -a 256 "$image" | tee "$backup_dir/SHA256SUMS"
echo "Display backup saved to $image"
