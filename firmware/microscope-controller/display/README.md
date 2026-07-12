# YD ESP TFT35 V4.1 display

The DLC32 MAX display is a separate controller connected to the mainboard through the RJ11 display port. The
Makerbase release image identifies it as `YD_V2.0.3_20241116`.

## Confirmed hardware state

- MCU: classic ESP32
- Flash: 4 MB, QIO, 80 MHz
- UI stack in the vendor binary: LVGL and TFT_eSPI
- Vendor image layout: bootloader at `0x1000`, partition table at `0x8000`, app at `0x10000`
- Partitions: 20 KB NVS, 3 MB OTA application, 960 KB SPIFFS
- Mainboard transport: UART, 115200 baud
- DLC32 MAX UART mapping used by this project: mainboard TX GPIO17, RX GPIO18

The LCD controller, touch controller and their GPIO mapping are not published and are not recoverable with enough
confidence from the stripped vendor image. Do not replace the display firmware until those signals have been
identified from the PCB or measured. The vendor image under the ignored Makerbase checkout remains the restore
source.

## Integration phases

1. Run the vendor display as a GRBL client through FluidNC `uart_channel1`.
2. Capture commands and status parsing behavior without changing the display.
3. Record PCB markings and display/touch GPIOs in a hardware profile.
4. Build a minimal LVGL hardware test with display, touch and UART only.
5. Add microscope views using the command contract in `protocol/stage-control.md`.

The final display UI will provide status, XY jog, Z focus, scan profile selection, scan progress, pause and cancel.
It must not contain independent motion state; all positions and machine states come from FluidNC.

The vendor firmware sends realtime `?` followed by proprietary byte `0xE1` and searches for the identity string
`Grbl_ESP32`. The mainboard startup macro sends that compatibility identity after UART initialization. FluidNC
answers the standard `?` request and handles `0xE1` as a display identity request. This explicit handling is also
required because an isolated `0xE1` would otherwise be interpreted as the start of a three-byte UTF-8 sequence.
