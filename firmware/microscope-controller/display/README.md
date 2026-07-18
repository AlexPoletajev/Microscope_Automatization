# YD ESP TFT35 V4.1 display

The DLC32 MAX display is a separate controller connected to the mainboard through the RJ11 display port. The
Makerbase release image identifies it as `YD_V2.0.3_20241116`.

## Confirmed hardware state

- MCU: classic ESP32
- Module: ESP32-WROOM-32U; observed silicon: ESP32-D0WD revision 1.1
- Flash: 4 MB, QIO, 80 MHz
- UI stack in the vendor binary: LVGL and TFT_eSPI
- Vendor image layout: bootloader at `0x1000`, partition table at `0x8000`, app at `0x10000`
- Partitions: NVS `0x9000` (20 KB), OTA data `0xe000` (8 KB), app `0x10000` (3 MB), SPIFFS
  `0x310000` (960 KB)
- USB bridge: CH340C
- Display bus buffer: 74HC125
- Touch controller: FocalTech FT62xx/FT63xx-compatible at I2C address `0x38`
- Touch bus: SDA GPIO0, SCL GPIO4, reset GPIO21, no interrupt pin
- LCD controller: ST7796, 480 x 320
- LCD SPI: MISO GPIO19, MOSI GPIO23, SCLK GPIO18, CS GPIO25, DC GPIO33, reset GPIO27
- Backlight: GPIO5, active low
- PCB revision: `ESP-TFT35 V4.1`
- Mainboard transport: UART, 115200 baud
- DLC32 MAX UART mapping used by this project: mainboard TX GPIO17, RX GPIO18

The panel mapping and backlight were verified by a custom color-bar firmware. The capacitive touch controller and
its direct landscape pixel transform (`x = rawY`, `y = 320 - rawX`) were verified with live corner and drag tests.
The complete original flash snapshot remains the restore source.

## Device backup

The original display was read in 64 KB chunks because long CH340 transfers were not reliable above 115200 baud.
The verified 4 MB snapshot has SHA-256
`f4549cc37e471ea13a3e1e2394db768fdb4cbcb2ea1d24d6a18c11a88a6a2a04` and is stored in the ignored local
`backups/display-20260713-095512` directory. Its first 64 KB also matched an independent read.

Run `scripts/backup_display.sh /dev/cu.usbserial-...` to create another retryable snapshot. AUX must either be
disconnected while the display has independent power, or the mainboard `uart1` hardware and `uart_channel1` must
be temporarily absent from its active configuration. Disabling automatic reports alone is insufficient because
the mainboard UART output and CH340 output remain electrically connected to the same display RX pin.

## Integration phases

1. Run the vendor display as a GRBL client through FluidNC `uart_channel1`.
2. Capture commands and status parsing behavior without changing the display.
3. Record PCB markings and display/touch GPIOs in a hardware profile.
4. Build a minimal LVGL hardware test with display, touch and UART only.
5. Add microscope views using the command contract in `protocol/stage-control.md`.

The display UI provides a full-screen XY pad, separate XY/ZA jog controls, a scan workflow and large settings
pages. The calibrated field size, camera resolution, overlap, speed, settling time, camera enable state and return
behavior are persisted in display NVS. Scan start and end positions deliberately live only for the current display
session and are cleared by a restart. The scan executor follows an endpoint-inclusive serpentine XY path and can
capture a configurable Z focus stack around the Z position present at scan start. It provides progress,
pause-at-next-frame and cancel controls. Each enabled camera capture pulses IO38
for a fixed 50 ms. It must not contain independent machine position state; all positions and motion states come
from FluidNC.

The vendor firmware sends realtime `?` followed by proprietary byte `0xE1` and searches for the identity string
`Grbl_ESP32`. The mainboard startup macro sends that compatibility identity after UART initialization. FluidNC
answers the standard `?` request and handles `0xE1` as a display identity request. This explicit handling is also
required because an isolated `0xE1` would otherwise be interpreted as the start of a three-byte UTF-8 sequence.
