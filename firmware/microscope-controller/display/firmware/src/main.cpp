#include <Arduino.h>
#include <TFT_eSPI.h>

namespace {
TFT_eSPI display;

constexpr uint16_t colors[] = {
    TFT_RED,
    TFT_GREEN,
    TFT_BLUE,
    TFT_CYAN,
    TFT_MAGENTA,
    TFT_YELLOW,
    TFT_WHITE,
    TFT_BLACK,
};

void drawTestPattern() {
    display.setRotation(1);
    const int16_t stripeWidth = display.width() / 8;

    for (size_t index = 0; index < 8; ++index) {
        display.fillRect(index * stripeWidth, 0, stripeWidth, display.height(), colors[index]);
    }

    display.fillRect(24, 112, display.width() - 48, 96, TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextDatum(MC_DATUM);
    display.drawString("ESP-TFT35 V4.1", display.width() / 2, 140, 2);
    display.drawString("ST7796 + XPT2046", display.width() / 2, 172, 2);
}
}  // namespace

void setup() {
    Serial.begin(115200);
    display.init();
    drawTestPattern();
    Serial.println("ESP-TFT35 hardware test ready");
}

void loop() {
    uint16_t x = 0;
    uint16_t y = 0;

    if (display.getTouchRaw(&x, &y)) {
        Serial.printf("touch raw x=%u y=%u z=%u\n", x, y, display.getTouchRawZ());
    }
    delay(50);
}
