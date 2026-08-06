#!/usr/bin/env python3
"""Identify the microscope controller and TFT USB serial adapters safely."""

from __future__ import annotations

import argparse
import sys
import time

import serial
from serial.tools import list_ports


def usb_serial_ports():
    ports = []
    for port in list_ports.comports():
        device = port.device.lower()
        description = (port.description or "").lower()
        if port.vid is not None or "usb" in device or "usb" in description or "ch340" in description:
            ports.append(port)
    return sorted(ports, key=lambda port: port.device)


def probe_fluidnc(device: str) -> tuple[bool, str]:
    connection = None
    try:
        connection = serial.Serial(device, 115200, timeout=0.1)
        time.sleep(0.3)
        connection.reset_input_buffer()
        deadline = time.monotonic() + 2.0
        next_query = 0.0
        response = bytearray()
        while time.monotonic() < deadline:
            now = time.monotonic()
            if now >= next_query:
                connection.write(b"?")
                next_query = now + 0.4
            response.extend(connection.read(connection.in_waiting or 1))
        text = response.decode("utf-8", errors="replace").strip()
        is_fluidnc = "<" in text and ("MPos:" in text or "WPos:" in text)
        return is_fluidnc, text
    except (OSError, serial.SerialException) as error:
        return False, f"nicht lesbar: {error}"
    finally:
        if connection is not None and connection.is_open:
            connection.close()


def format_usb_id(port) -> str:
    if port.vid is None or port.pid is None:
        return "VID/PID unbekannt"
    return f"VID:PID {port.vid:04X}:{port.pid:04X}"


def inspect_once() -> int:
    ports = usb_serial_ports()
    print(f"USB-Seriell-Geräte: {len(ports)}")
    if not ports:
        print("FEHLT: Weder Hauptcontroller noch Display werden vom Betriebssystem erkannt.")
        return 2

    controller_count = 0
    silent_ports = []
    for port in ports:
        is_controller, response = probe_fluidnc(port.device)
        role = "HAUPTCONTROLLER (FluidNC)" if is_controller else "kein FluidNC; moeglicherweise DISPLAY"
        print(f"- {port.device}: {role}")
        print(f"  {port.description or 'ohne Beschreibung'}; {format_usb_id(port)}")
        if response:
            first_line = response.splitlines()[0]
            print(f"  Antwort: {first_line[:160]}")
        if is_controller:
            controller_count += 1
        else:
            silent_ports.append(port.device)

    if controller_count >= 1 and silent_ports:
        print("OK: Hauptcontroller und ein separater Display-Kandidat sind sichtbar.")
        print(f"Display-Kandidat: {silent_ports[0]}")
        return 0
    if controller_count >= 1:
        print("FEHLT: FluidNC ist sichtbar, aber kein separater Display-USB-Port.")
        return 1
    if len(ports) == 1:
        print("HINWEIS: Ein stilles USB-Gerät ist sichtbar; vermutlich das Display, aber FluidNC fehlt.")
        return 1
    print("HINWEIS: Mehrere USB-Geräte sind sichtbar, aber FluidNC wurde nicht erkannt.")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Controller und TFT ueber USB erkennen, ohne zu flashen oder Bewegung auszulösen"
    )
    parser.add_argument("--watch", action="store_true", help="Erkennung alle zwei Sekunden wiederholen")
    args = parser.parse_args()
    if not args.watch:
        return inspect_once()
    try:
        while True:
            print("\n--- USB-Pruefung ---")
            inspect_once()
            time.sleep(2)
    except KeyboardInterrupt:
        print("\nBeendet.")
        return 0


if __name__ == "__main__":
    sys.exit(main())
