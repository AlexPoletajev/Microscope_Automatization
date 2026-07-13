# ESP-TFT35 V4.1 hardware profile

## Confirmed

| Function | Device or connection |
| --- | --- |
| MCU module | ESP32-WROOM-32U |
| USB bridge | CH340C |
| Display buffer | 74HC125 |
| Touch controller | FocalTech FT62xx/FT63xx-compatible, I2C address `0x38` |
| Panel controller | ST7796, 480 x 320 |
| Backlight | GPIO5, active low |
| Mainboard link | ESP32 UART0 shared with CH340, 115200 8N1 |
| Flash | 4 MB, QIO, 80 MHz |

## Display mapping

The panel mapping, backlight and capacitive touch interface were verified on the physical display.

| Signal | GPIO | Confidence |
| --- | ---: | --- |
| TFT MISO | 19 | high |
| TFT MOSI | 23 | high |
| TFT SCLK | 18 | high |
| TFT CS | 25 | confirmed |
| TFT DC | 33 | confirmed |
| TFT reset | 27 | confirmed |
| Backlight | 5, active low | confirmed |
| Touch SDA | 0 | confirmed |
| Touch SCL | 4 | confirmed |
| Touch reset | 21 | confirmed |
| Touch interrupt | not connected (`-1`) | recovered from vendor firmware |
| AUX RX | 3 / UART0 RX | confirmed by USB/AUX contention |
| AUX TX | 1 / UART0 TX | confirmed by USB/AUX contention |

Confirmed panel controller: ST7796, 480 x 320 in landscape orientation.

The touch controller reports coordinates in the panel's native portrait orientation. Landscape screen pixels are
`x = rawY` and `y = 320 - rawX`; no additional scaling or calibration is required.
