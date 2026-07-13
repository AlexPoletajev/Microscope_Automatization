# ESP-TFT35 V4.1 hardware profile

## Confirmed

| Function | Device or connection |
| --- | --- |
| MCU module | ESP32-WROOM-32U |
| USB bridge | CH340C |
| Display buffer | 74HC125 |
| Touch controller | XPT2046-compatible marking |
| Mainboard link | ESP32 UART0 shared with CH340, 115200 8N1 |
| Flash | 4 MB, QIO, 80 MHz |

## Candidate display mapping

This mapping comes from the closest Makerbase TFT_eSPI profile in the available DLC32 source. It must be proven by
the minimal hardware test before application UI work begins.

| Signal | GPIO | Confidence |
| --- | ---: | --- |
| TFT MISO | 19 | high |
| TFT MOSI | 23 | high |
| TFT SCLK | 18 | high |
| TFT CS | 25 | medium |
| TFT DC | 33 | medium |
| TFT reset | 27 | medium |
| Touch CS | 26 | high |
| AUX RX | 3 / UART0 RX | confirmed by USB/AUX contention |
| AUX TX | 1 / UART0 TX | confirmed by USB/AUX contention |

Candidate panel controller: ST7796, 480 x 320 in landscape orientation.
