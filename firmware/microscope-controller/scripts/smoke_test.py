#!/usr/bin/env python3
import argparse
import time

import serial


def command(connection, value, timeout=3):
    connection.write((value + "\n").encode("ascii"))
    deadline = time.monotonic() + timeout
    response = []
    while time.monotonic() < deadline:
        line = connection.readline().decode("utf-8", errors="replace").strip()
        if not line:
            continue
        response.append(line)
        if line == "ok" or line.startswith(("error:", "ALARM:")):
            break
    print(f"> {value}")
    print("\n".join(response))
    if not response or response[-1] != "ok":
        raise RuntimeError(f"Command failed: {value}")


def main():
    parser = argparse.ArgumentParser(description="Conservative 1 mm XY motion smoke test")
    parser.add_argument("port")
    parser.add_argument("--move", action="store_true", help="Required acknowledgement that motors may move")
    args = parser.parse_args()
    if not args.move:
        parser.error("Pass --move after checking that the stage has at least 2 mm clearance in every direction")

    with serial.Serial(args.port, 115200, timeout=0.2) as connection:
        time.sleep(1.5)
        connection.reset_input_buffer()
        for value in ("$X", "G21", "G91", "G1 X1 F60", "G1 X-1 F60", "G1 Y1 F60", "G1 Y-1 F60", "G90"):
            command(connection, value)


if __name__ == "__main__":
    main()
