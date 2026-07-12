#!/usr/bin/env python3
import argparse
import getpass
import time

import serial


def send_command(connection, value):
    connection.write((value + "\n").encode("utf-8"))
    deadline = time.monotonic() + 3
    response = []
    while time.monotonic() < deadline:
        line = connection.readline().decode("utf-8", errors="replace").strip()
        if not line:
            continue
        response.append(line)
        if line == "ok" or line.startswith("error:"):
            break
    if not response or response[-1] != "ok":
        raise RuntimeError(f"{value.split('=', 1)[0]} failed: {response}")


def main():
    parser = argparse.ArgumentParser(description="Configure FluidNC AP and optional home Wi-Fi")
    parser.add_argument("port")
    parser.add_argument("--station-ssid", help="Home Wi-Fi SSID; omit to keep AP-only operation")
    parser.add_argument("--ap-name", default="MicroscopeStage")
    parser.add_argument("--hostname", default="microscope-stage")
    args = parser.parse_args()

    station_password = getpass.getpass("Home Wi-Fi password: ") if args.station_ssid else None
    with serial.Serial(args.port, 115200, timeout=0.25) as connection:
        time.sleep(1.5)
        connection.reset_input_buffer()
        send_command(connection, f"$AP/SSID={args.ap_name}")
        send_command(connection, f"$Hostname={args.hostname}")
        if args.station_ssid:
            send_command(connection, f"$Sta/SSID={args.station_ssid}")
            send_command(connection, f"$Sta/Password={station_password}")
            send_command(connection, "$WiFi/Mode=STA>AP")
        else:
            send_command(connection, "$WiFi/Mode=AP")
        send_command(connection, "$System/Control=RESTART")
    print("Wi-Fi configuration saved; controller is restarting")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, serial.SerialException) as error:
        raise SystemExit(f"Wi-Fi configuration failed: {error}")
