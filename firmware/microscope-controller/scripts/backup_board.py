#!/usr/bin/env python3
import argparse
import datetime
import glob
import json
import pathlib
import shutil
import subprocess
import sys
import time

import serial


def detect_port():
    candidates = sorted(glob.glob("/dev/cu.usbserial-*") + glob.glob("/dev/cu.wchusbserial*"))
    if len(candidates) != 1:
        raise RuntimeError(f"Expected one serial board, found: {candidates or 'none'}")
    return candidates[0]


def capture_settings(port):
    responses = {}
    with serial.Serial(port, 115200, timeout=0.4) as connection:
        time.sleep(1.5)
        connection.reset_input_buffer()
        for command in ("$I", "$$"):
            connection.write((command + "\n").encode("ascii"))
            lines = []
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                line = connection.readline().decode("utf-8", errors="replace").strip()
                if not line:
                    continue
                lines.append(line)
                if line == "ok" or line.startswith("error:"):
                    break
            responses[command] = lines
    return responses


def main():
    project_dir = pathlib.Path(__file__).resolve().parents[1]
    default_fallback = project_dir.parent / "MKS-DLC32-MAX-extracted" / "LAS_MKS_DLC32_Max_V1.0.10_20250113_Beta" / "LAS_MKS_DLC32_Max_V1.0.10_20250113_Beta.bin"
    parser = argparse.ArgumentParser(description="Back up DLC32 MAX settings and restore data")
    parser.add_argument("--port", help="Serial port; auto-detected when omitted")
    parser.add_argument("--baud", default="115200", help="Esptool baud rate (default: 115200 for reliable reads)")
    parser.add_argument("--full", action="store_true", help="Read the complete 8 MB flash instead of the device data header")
    parser.add_argument("--fallback-image", type=pathlib.Path, default=default_fallback,
                        help="Known-good Makerbase image copied into the backup")
    args = parser.parse_args()
    port = args.port or detect_port()
    timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    output_dir = project_dir / "backups" / timestamp
    output_dir.mkdir(parents=True)

    settings = capture_settings(port)
    (output_dir / "settings.json").write_text(json.dumps(settings, indent=2, ensure_ascii=True) + "\n")

    output_name = "flash.bin" if args.full else "board-header-nvs.bin"
    read_size = "0x800000" if args.full else "0x10000"
    subprocess.run(
        [sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", port,
         "--baud", args.baud, "read-flash", "0x0", read_size, str(output_dir / output_name)],
        check=True,
    )
    if not args.fallback_image.is_file():
        raise RuntimeError(f"Makerbase fallback image not found: {args.fallback_image}")
    shutil.copy2(args.fallback_image, output_dir / "restore-makerbase.bin")
    print(output_dir)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, serial.SerialException, subprocess.CalledProcessError) as error:
        print(f"Backup failed: {error}", file=sys.stderr)
        raise SystemExit(1)
