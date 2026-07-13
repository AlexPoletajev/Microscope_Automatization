# ESP-TFT35 V4.1 hardware profile

## Confirmed

| Function | Device or connection |
| --- | --- |
| MCU module | ESP32-WROOM-32U |
| USB bridge | CH340C |
| Display buffer | 74HC125 |
| Touch controller | XPT2046-compatible marking |
| Panel controller | ST7796, 480 x 320 |
| Backlight | GPIO5, active low |
| Mainboard link | ESP32 UART0 shared with CH340, 115200 8N1 |
| Flash | 4 MB, QIO, 80 MHz |

## Display mapping

The panel mapping and backlight were verified with the minimal hardware test. Touch CS still needs to be verified
with an active touch measurement.

| Signal | GPIO | Confidence |
| --- | ---: | --- |
| TFT MISO | 19 | high |
| TFT MOSI | 23 | high |
| TFT SCLK | 18 | high |
| TFT CS | 25 | confirmed |
| TFT DC | 33 | confirmed |
| TFT reset | 27 | confirmed |
| Backlight | 5, active low | confirmed |
| Touch CS | 26 | candidate |
| AUX RX | 3 / UART0 RX | confirmed by USB/AUX contention |
| AUX TX | 1 / UART0 TX | confirmed by USB/AUX contention |

Confirmed panel controller: ST7796, 480 x 320 in landscape orientation.
