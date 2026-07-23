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

The display UI provides a full-screen XY pad, separate XY/ZA jog controls, a scan workflow, a repeatability/slip
test and large settings pages. The calibrated field size, camera resolution, overlap range, separate XY scan speed
up to 2000 mm/min and Z scan speed up to 12000 mm/min, settling time, camera enable state, timing-marker mode and
return behavior are persisted in display
NVS. Scan start and end positions are captured independently for X and Y, deliberately live only for the current
display session and are cleared by a restart. Changing one axis endpoint never overwrites an endpoint of the other
axis. When more than one focus step is selected, the Z start and end positions are
also captured in the scan workflow and remain session-only. The scan executor follows an endpoint-inclusive
serpentine XY path and distributes the configured focus steps evenly between those Z endpoints. It provides progress,
pause-at-next-frame and cancel controls. The workflow and progress view show the X-column, Y-row, Z-focus and total
image counts so the acquisition grid can be reconstructed for stitching. X and Y use an equal endpoint-inclusive
stride whenever an integer division falls inside the configured overlap range. Otherwise only the final edge step
is shortened and the grid summary marks that axis with `RAND`. Each enabled camera capture pulses IO38 once for
a fixed 15 ms and waits at least 100 ms after returning the output low. No display redraw is performed while the
trigger is high, so rendering cannot extend the electrical pulse. It must not contain independent machine position state; all positions and motion
states come from FluidNC.

Both manual XY control surfaces allow a displayed feed up to 600 mm/min. The ZA cross control retains the configured
10:1 Z translation multiplier shown below its speed slider.

The Tests page contains a complete scan overview, scan-step, field-width, scan-range and slip tests. The overview
shows field calibration, XYZ ranges and steps, requested and actual overlap, edge exceptions, raster dimensions,
image total, separate XY/Z speeds, settling, camera pulse, resolution and return behavior. The step and field-width tests move
X/Y/Z by the corresponding incremental or complete distance. The scan-range test provides six absolute controls
for the X, Y and Z start/end coordinates. The slip test captures two session-only XYZ points, including
the focus Z position at each point. It moves to A first and then performs the configured number of A-B-A rounds at
the saved test speed, always ending at A. Test speed and round count persist in display NVS; stopping aborts motion.

The settings page also provides a persistent scan history. Only successfully completed scans are recorded; stopped
or failed scans are omitted. A 32-entry NVS ring stores the defined XYZ endpoints, duration, field calibration,
requested and achieved overlap, raster and image counts, XYZ steps and feeds, settling and camera timing,
resolution, return behavior and edge-step flags. The newest records are shown first and the oldest record is
overwritten when the ring is full. The display has no real-time clock, so records use a persistent sequence number
and measured duration rather than an unreliable wall-clock timestamp.

The optional timing-marker mode accounts for cameras whose EXIF timestamps only have whole-second resolution.
Ordinary Z-stack transitions retain the normal camera recovery interval, completed stacks enforce an 8-second
minimum interval before the next X position, and completed serpentine rows enforce 20 seconds before the next Y
position. This creates three separable timestamp classes without extra marker images. The setting persists in NVS,
is visible in the scan overview and is copied into each completed history record.

Focus stacks also follow a serpentine Z order. The first XY position runs from focus start to focus end, the next
position runs from focus end back to focus start, and the direction continues alternating at every XY position.
This removes the full Z reset between neighboring stacks while preserving the same image count and focus levels.
The Z-order mode is shown in the scan overview and stored with completed history entries.

All touch-and-hold jog controls continuously repeat bounded motion segments while pressed. On touch release, the
display repeats the realtime jog cancel command until FluidNC reports `Idle`, covering the XY pad, the XY/ZA arrows
and field-calibration arrows.

The vendor firmware sends realtime `?` followed by proprietary byte `0xE1` and searches for the identity string
`Grbl_ESP32`. The mainboard startup macro sends that compatibility identity after UART initialization. FluidNC
answers the standard `?` request and handles `0xE1` as a display identity request. This explicit handling is also
required because an isolated `0xE1` would otherwise be interpreted as the start of a three-byte UTF-8 sequence.
