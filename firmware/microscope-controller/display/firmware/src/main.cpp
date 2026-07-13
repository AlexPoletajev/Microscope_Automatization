#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>

namespace {
TFT_eSPI display;

constexpr int touchSda = 0;
constexpr int touchScl = 4;
constexpr int touchReset = 21;
constexpr uint8_t touchAddress = 0x38;

constexpr int16_t padX = 16;
constexpr int16_t padY = 66;
constexpr int16_t padWidth = 330;
constexpr int16_t padHeight = 238;

struct TouchPoint {
    uint16_t x;
    uint16_t y;
};

bool readTouch(TouchPoint& point) {
    Wire.beginTransmission(touchAddress);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(touchAddress, static_cast<uint8_t>(5)) != 5) {
        return false;
    }

    const uint8_t touches = Wire.read() & 0x0F;
    const uint8_t xHigh = Wire.read();
    const uint8_t xLow = Wire.read();
    const uint8_t yHigh = Wire.read();
    const uint8_t yLow = Wire.read();
    if (touches == 0) {
        return false;
    }

    point.x = (static_cast<uint16_t>(xHigh & 0x0F) << 8) | xLow;
    point.y = (static_cast<uint16_t>(yHigh & 0x0F) << 8) | yLow;
    return true;
}

void drawShell() {
    display.setRotation(1);
    display.fillScreen(TFT_BLACK);
    display.fillRect(0, 0, display.width(), 52, TFT_DARKGREY);
    display.setTextDatum(ML_DATUM);
    display.setTextColor(TFT_WHITE, TFT_DARKGREY);
    display.drawString("MICROSCOPE STAGE", 18, 26, 2);
    display.setTextDatum(MR_DATUM);
    display.setTextColor(TFT_CYAN, TFT_DARKGREY);
    display.drawString("TOUCH TEST", display.width() - 18, 26, 2);

    display.drawRoundRect(padX, padY, padWidth, padHeight, 4, TFT_DARKGREY);
    display.drawFastVLine(padX + padWidth / 2, padY + 1, padHeight - 2, TFT_DARKGREY);
    display.drawFastHLine(padX + 1, padY + padHeight / 2, padWidth - 2, TFT_DARKGREY);

    display.setTextDatum(ML_DATUM);
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    display.drawString("Touch", 366, 88, 2);
    display.drawString("Raw X", 366, 136, 1);
    display.drawString("Raw Y", 366, 188, 1);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.drawString("Keine", 366, 108, 1);
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    display.drawString("Stage-", 366, 258, 1);
    display.drawString("Befehle", 366, 274, 1);
}

void drawValue(int16_t y, uint16_t value) {
    display.fillRect(364, y, 108, 28, TFT_BLACK);
    display.setTextDatum(ML_DATUM);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawNumber(value, 366, y + 14, 2);
}
}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    display.init();
    drawShell();

    pinMode(touchReset, OUTPUT);
    digitalWrite(touchReset, LOW);
    delay(20);
    digitalWrite(touchReset, HIGH);
    delay(100);
    Wire.begin(touchSda, touchScl, 400000);
    Serial.println("FT62XX_TOUCH_READY");
}

void loop() {
    static bool wasTouched = false;
    static int16_t lastDotX = -1;
    static int16_t lastDotY = -1;
    static uint32_t lastTelemetryAt = 0;
    TouchPoint point{};
    const bool touched = readTouch(point);

    if (touched) {
        const int16_t dotX = constrain(static_cast<int16_t>(point.y), padX + 8, padX + padWidth - 8);
        const int16_t dotY = constrain(static_cast<int16_t>(320 - point.x), padY + 8, padY + padHeight - 8);

        if ((dotX != lastDotX || dotY != lastDotY) && lastDotX >= 0) {
            display.fillCircle(lastDotX, lastDotY, 7, TFT_BLACK);
        }
        if (dotX != lastDotX || dotY != lastDotY) {
            lastDotX = dotX;
            lastDotY = dotY;
            display.fillCircle(lastDotX, lastDotY, 7, TFT_CYAN);
        }
        if (millis() - lastTelemetryAt >= 150) {
            drawValue(144, point.x);
            drawValue(196, point.y);
            lastTelemetryAt = millis();
        }
        if (!wasTouched) {
            display.fillRect(364, 100, 108, 20, TFT_BLACK);
            display.setTextColor(TFT_GREEN, TFT_BLACK);
            display.setTextDatum(ML_DATUM);
            display.drawString("Aktiv", 366, 110, 1);
        }
    } else if (wasTouched) {
        display.fillRect(364, 100, 108, 20, TFT_BLACK);
        display.setTextColor(TFT_YELLOW, TFT_BLACK);
        display.setTextDatum(ML_DATUM);
        display.drawString("Keine", 366, 110, 1);
    }

    wasTouched = touched;
    delay(2);
}
