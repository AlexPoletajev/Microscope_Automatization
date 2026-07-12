# Microscope Stage Controller

This directory contains the first DLC32 MAX firmware milestone for direct XY control of the microscope stage.
It uses the pinned FluidNC submodule as its motion core and replaces FluidNC's filesystem contents at build time
with the machine configuration and the installable control web app.

The web app now contains control, scan-profile and system foundations. Scan execution remains locked by
`web/machine-profile.json` until axis calibration and the camera trigger are commissioned. The vendor display is
connected as a 115200-baud GRBL client on UART1; its replacement-firmware investigation is documented under
`display/`.

## Safety state

The deployed `config.yaml` only defines X and Y. Z and A are documented in `config-4axis.yaml`, but that file is
not copied to the firmware image. Homing, hard limits and soft limits are disabled until physical endstops and
travel dimensions are known. After every restart, displayed coordinates are relative and must not be treated as
an absolute physical position.

The web app's stop button sends FluidNC's jog-cancel command. The controller reset button sends a software reset.
Neither replaces a physical emergency stop that disconnects motor power.

## Build

```sh
git submodule update --init --recursive
cd firmware/microscope-controller
python3 -m pip install -r requirements.txt
./scripts/build.sh
```

The merged 8 MB image is written to `build/microscope-stage-merged.bin`. The build script temporarily stages the
controller files inside FluidNC and restores the submodule afterwards.

After the initial installation, configuration and web-app changes should be written without erasing Wi-Fi and NVS:

```sh
./scripts/build.sh
./scripts/update_filesystem.sh /dev/cu.usbserial-210
```

This updates only the LittleFS partition at `0x610000`. Use the full `flash.sh` only for firmware-core updates or
recovery because it replaces the complete flash, including saved Wi-Fi settings.

## Back up and flash

Connect the board by USB while it is still running the Makerbase firmware:

```sh
./scripts/backup_board.py --port /dev/cu.usbserial-210
./scripts/flash.sh /dev/cu.usbserial-210
```

Flashing is refused until a backup set exists under the ignored `backups/` directory. It contains the Makerbase
settings, the device-specific 64 KB boot/partition/NVS region and the known-good Makerbase restore image. A full
8 MB read can optionally be requested with `backup_board.py --full`, but is substantially slower and more
sensitive to USB serial noise.

On first boot FluidNC provides the access point `FluidNC` with password `12345678`. Rename it and optionally add
the home Wi-Fi credentials over USB:

```sh
./scripts/configure_wifi.py /dev/cu.usbserial-210 --station-ssid "My Wi-Fi"
```

The password is requested without echo. The controller tries the configured home network and falls back to its
own `MicroscopeStage` access point when that network is unavailable. In the home network the normal address is
`http://microscope-stage.local`.

## First motion test

Check that X and Y have at least 2 mm clearance in both directions. Then run:

```sh
./scripts/smoke_test.py /dev/cu.usbserial-210 --move
```

The test moves each axis 1 mm forward and back at 60 mm/min. Exact `steps_per_mm`, direction, acceleration and
travel limits must be calibrated before increasing the configured 300 mm/min ceiling.

## Web app

FluidNC starts its default access point after flashing. Open the controller address in Safari or Chrome. The app
can be installed from the browser's Add to Home Screen action. Finger direction on the XY pad sets the movement
vector; distance from the center sets speed. Releasing the pad, hiding the app or losing the WebSocket connection
cancels jogging.
