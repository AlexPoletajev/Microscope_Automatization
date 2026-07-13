#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>

#include <cmath>
#include <cstring>

namespace {
TFT_eSPI display;
HardwareSerial controllerSerial(2);

constexpr int touchSda = 0;
constexpr int touchScl = 4;
constexpr int touchReset = 21;
constexpr uint8_t touchAddress = 0x38;

constexpr int16_t railWidth = 52;
constexpr int16_t padX = 62;
constexpr int16_t padY = 10;
constexpr int16_t padSize = 300;
constexpr int16_t padCenterX = padX + padSize / 2;
constexpr int16_t padCenterY = padY + padSize / 2;
constexpr int16_t sliderPanelX = 372;
constexpr int16_t sliderCenterX = 426;
constexpr int16_t sliderTop = 58;
constexpr int16_t sliderBottom = 250;
constexpr float deadzone = 0.12F;
constexpr int minFeed = 30;
constexpr int maxFeedLimit = 300;
constexpr uint32_t statusRequestIntervalMs = 250;
constexpr uint32_t statusTimeoutMs = 1000;
constexpr uint32_t jogIntervalMs = 110;
constexpr uint32_t jogHorizonMs = 190;
constexpr uint8_t jogCancel = 0x85;

int maxFeed = 60;

uint16_t colorBackground;
uint16_t colorSurface;
uint16_t colorRaised;
uint16_t colorPad;
uint16_t colorLine;
uint16_t colorMuted;
uint16_t colorText;
uint16_t colorCyan;
uint16_t colorCyanDark;
uint16_t colorGreen;
uint16_t colorAmber;
uint16_t colorRed;

struct TouchPoint {
    uint16_t x;
    uint16_t y;
};

struct PadState {
    int16_t handleX = padCenterX;
    int16_t handleY = padCenterY;
    float vectorX = 0.0F;
    float vectorY = 0.0F;
    float magnitude = 0.0F;
    int speed = 0;
    bool active = false;
};

enum class TouchMode { None, Pad, Speed };
enum class MachineState { Unknown, Ready, Blocked };
enum class ConnectionState { Offline, Ready, Blocked };

PadState padState;
MachineState machineState = MachineState::Unknown;
ConnectionState drawnConnectionState = ConnectionState::Offline;
uint32_t lastStatusAt = 0;
uint32_t lastStatusRequestAt = 0;
uint32_t lastJogAt = 0;
bool receivedStatus = false;
bool jogCommandActive = false;
char statusFrame[48];
size_t statusFrameLength = 0;
bool receivingStatusFrame = false;

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

void drawMenuIcon(int16_t centerX, int16_t centerY, uint16_t color) {
    for (int offset = -7; offset <= 7; offset += 7) {
        display.drawFastHLine(centerX - 9, centerY + offset, 18, color);
    }
}

void drawControlIcon(int16_t centerX, int16_t centerY, uint16_t color) {
    display.drawCircle(centerX, centerY, 9, color);
    display.drawFastHLine(centerX - 13, centerY, 26, color);
    display.drawFastVLine(centerX, centerY - 13, 26, color);
    display.fillCircle(centerX, centerY, 2, color);
}

void drawScanIcon(int16_t centerX, int16_t centerY, uint16_t color) {
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            display.drawRect(centerX - 10 + column * 12, centerY - 10 + row * 12, 8, 8, color);
        }
    }
}

void drawSettingsIcon(int16_t centerX, int16_t centerY, uint16_t color) {
    display.drawFastHLine(centerX - 11, centerY - 7, 22, color);
    display.drawFastHLine(centerX - 11, centerY, 22, color);
    display.drawFastHLine(centerX - 11, centerY + 7, 22, color);
    display.fillCircle(centerX - 4, centerY - 7, 3, colorSurface);
    display.drawCircle(centerX - 4, centerY - 7, 3, color);
    display.fillCircle(centerX + 5, centerY, 3, colorSurface);
    display.drawCircle(centerX + 5, centerY, 3, color);
    display.fillCircle(centerX - 1, centerY + 7, 3, colorSurface);
    display.drawCircle(centerX - 1, centerY + 7, 3, color);
}

void drawRailButton(int16_t y, bool selected, void (*drawIcon)(int16_t, int16_t, uint16_t)) {
    if (selected) {
        display.fillRoundRect(7, y, 38, 38, 4, colorRaised);
        display.fillRect(0, y + 7, 3, 24, colorCyan);
    }
    drawIcon(26, y + 19, selected ? colorCyan : colorMuted);
}

void drawRail() {
    display.fillRect(0, 0, railWidth, 320, colorSurface);
    display.drawFastVLine(railWidth - 1, 0, 320, colorLine);
    drawRailButton(8, false, drawMenuIcon);
    drawRailButton(68, true, drawControlIcon);
    drawRailButton(120, false, drawScanIcon);
    drawRailButton(274, false, drawSettingsIcon);
}

ConnectionState connectionState() {
    if (!receivedStatus || millis() - lastStatusAt > statusTimeoutMs) {
        return ConnectionState::Offline;
    }
    return machineState == MachineState::Ready ? ConnectionState::Ready : ConnectionState::Blocked;
}

void drawConnectionIndicator(bool force = false) {
    const ConnectionState state = connectionState();
    if (!force && state == drawnConnectionState) {
        return;
    }

    const uint16_t color = state == ConnectionState::Ready
        ? colorGreen
        : (state == ConnectionState::Blocked ? colorAmber : colorRed);
    constexpr uint16_t readyGreen = static_cast<uint16_t>((TFT_GREEN << 8) | (TFT_GREEN >> 8));
    display.fillCircle(26, 258, 6, colorSurface);
    display.fillCircle(26, 258, 4, state == ConnectionState::Ready ? readyGreen : color);
    drawnConnectionState = state;
}

void drawPadGrid() {
    for (int index = 1; index < 10; ++index) {
        const int16_t offset = index * padSize / 10;
        const uint16_t lineColor = index == 5 ? colorMuted : colorLine;
        display.drawFastVLine(padX + offset, padY + 1, padSize - 2, lineColor);
        display.drawFastHLine(padX + 1, padY + offset, padSize - 2, lineColor);
    }
    display.drawCircle(padCenterX, padCenterY, static_cast<int16_t>(padSize * deadzone), colorMuted);
    display.fillCircle(padCenterX, padCenterY, 2, colorMuted);
}

void drawPadFrame() {
    display.fillRoundRect(padX, padY, padSize, padSize, 5, colorPad);
    display.drawRoundRect(padX, padY, padSize, padSize, 5, colorLine);
    drawPadGrid();

    display.setTextColor(colorMuted, colorPad);
    display.setTextDatum(TC_DATUM);
    display.drawString("Y+", padCenterX, padY + 8, 1);
    display.setTextDatum(MR_DATUM);
    display.drawString("X+", padX + padSize - 8, padCenterY, 1);
}

void drawHandle() {
    if (padState.active && padState.speed > 0) {
        display.drawLine(padCenterX, padCenterY, padState.handleX, padState.handleY, colorCyan);
    }
    display.fillCircle(padState.handleX, padState.handleY, 13, colorCyanDark);
    display.drawCircle(padState.handleX, padState.handleY, 13, colorText);
    display.drawCircle(padState.handleX, padState.handleY, 12, colorCyan);
}

void eraseDynamicPad() {
    display.drawLine(padCenterX, padCenterY, padState.handleX, padState.handleY, colorPad);
    display.fillCircle(padState.handleX, padState.handleY, 14, colorPad);
    drawPadGrid();
}

void drawSpeedSlider() {
    display.fillRect(sliderPanelX, 0, 108, 320, colorSurface);
    display.drawFastVLine(sliderPanelX, 0, 320, colorLine);

    display.setTextDatum(TC_DATUM);
    display.setTextColor(colorText, colorSurface);
    display.drawString("TEMPO", sliderCenterX, 14, 2);
    display.setTextColor(colorMuted, colorSurface);
    display.drawNumber(maxFeedLimit, sliderCenterX, sliderTop - 16, 1);
    display.drawNumber(minFeed, sliderCenterX, sliderBottom + 13, 1);

    display.fillRoundRect(sliderCenterX - 3, sliderTop, 6, sliderBottom - sliderTop, 3, colorLine);
    const int16_t knobY = map(maxFeed, minFeed, maxFeedLimit, sliderBottom, sliderTop);
    display.fillRoundRect(sliderCenterX - 3, knobY, 6, sliderBottom - knobY, 3, colorCyan);
    for (int index = 0; index <= 6; ++index) {
        const int16_t y = sliderTop + index * (sliderBottom - sliderTop) / 6;
        display.drawFastHLine(sliderCenterX - 16, y, 7, colorMuted);
        display.drawFastHLine(sliderCenterX + 10, y, 7, colorMuted);
    }
    display.fillCircle(sliderCenterX, knobY, 14, colorRaised);
    display.drawCircle(sliderCenterX, knobY, 14, colorText);
    display.drawCircle(sliderCenterX, knobY, 13, colorCyan);

    display.fillRect(sliderPanelX + 8, 278, 92, 34, colorSurface);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(colorText, colorSurface);
    display.drawNumber(maxFeed, sliderCenterX, 278, 2);
    display.setTextColor(colorMuted, colorSurface);
    display.drawString("mm/min", sliderCenterX, 298, 1);
}

void updatePad(int16_t screenX, int16_t screenY) {
    float x = static_cast<float>(screenX - padCenterX) / (padSize / 2);
    float y = static_cast<float>(padCenterY - screenY) / (padSize / 2);
    const float rawMagnitude = std::sqrt(x * x + y * y);
    if (rawMagnitude > 1.0F) {
        x /= rawMagnitude;
        y /= rawMagnitude;
    }
    const float magnitude = min(1.0F, rawMagnitude);
    const float scaledMagnitude = magnitude <= deadzone ? 0.0F : (magnitude - deadzone) / (1.0F - deadzone);

    eraseDynamicPad();
    padState.handleX = padCenterX + static_cast<int16_t>(x * magnitude * (padSize / 2 - 14));
    padState.handleY = padCenterY - static_cast<int16_t>(y * magnitude * (padSize / 2 - 14));
    padState.vectorX = scaledMagnitude > 0.0F ? x : 0.0F;
    padState.vectorY = scaledMagnitude > 0.0F ? y : 0.0F;
    padState.magnitude = scaledMagnitude;
    padState.speed = static_cast<int>(maxFeed * scaledMagnitude + 0.5F);
    padState.active = true;
    drawHandle();
}

void releasePad() {
    eraseDynamicPad();
    padState = PadState{};
    drawHandle();
}

void cancelJog() {
    controllerSerial.write(jogCancel);
    jogCommandActive = false;
}

void sendJogSegment() {
    if (!padState.active || padState.speed <= 0 || connectionState() != ConnectionState::Ready) {
        if (jogCommandActive) {
            cancelJog();
        }
        return;
    }
    const float feed = static_cast<float>(max(1, padState.speed));
    const float distance = feed * jogHorizonMs / 60000.0F;
    String command;
    command.reserve(64);
    command = F("$J=G91 G21 X");
    command += String(distance * padState.vectorX, 4);
    command += F(" Y");
    command += String(distance * padState.vectorY, 4);
    command += F(" F");
    command += String(feed, 1);
    command += '\r';
    controllerSerial.write(reinterpret_cast<const uint8_t*>(command.c_str()), command.length());
    controllerSerial.flush();
    jogCommandActive = true;
    lastJogAt = millis();
}

void parseStatusFrame() {
    statusFrame[statusFrameLength] = '\0';
    char* separator = strchr(statusFrame, '|');
    if (separator != nullptr) {
        *separator = '\0';
    }

    machineState = strcmp(statusFrame, "Idle") == 0 || strcmp(statusFrame, "Jog") == 0
        ? MachineState::Ready
        : MachineState::Blocked;
    receivedStatus = true;
    lastStatusAt = millis();
}

void receiveControllerData() {
    while (controllerSerial.available() > 0) {
        const char incoming = static_cast<char>(controllerSerial.read());
        if (incoming == '<') {
            receivingStatusFrame = true;
            statusFrameLength = 0;
            continue;
        }
        if (!receivingStatusFrame) {
            continue;
        }
        if (incoming == '>') {
            parseStatusFrame();
            receivingStatusFrame = false;
            statusFrameLength = 0;
            continue;
        }
        if (incoming == '|') {
            parseStatusFrame();
            receivingStatusFrame = false;
            statusFrameLength = 0;
            continue;
        }
        if (statusFrameLength < sizeof(statusFrame) - 1) {
            statusFrame[statusFrameLength++] = incoming;
        } else {
            receivingStatusFrame = false;
            statusFrameLength = 0;
        }
    }
}

void serviceController() {
    receiveControllerData();
    const uint32_t now = millis();
    if (now - lastStatusRequestAt >= statusRequestIntervalMs) {
        controllerSerial.write('?');
        lastStatusRequestAt = now;
    }
    if (padState.active && now - lastJogAt >= jogIntervalMs) {
        sendJogSegment();
    }
    drawConnectionIndicator();
}

bool insidePad(int16_t x, int16_t y) {
    return x >= padX && x < padX + padSize && y >= padY && y < padY + padSize;
}

bool insideSpeedSlider(int16_t x, int16_t y) {
    return x >= sliderPanelX && y >= sliderTop - 20 && y <= sliderBottom + 20;
}

void updateSpeed(int16_t screenY) {
    const int16_t constrainedY = constrain(screenY, sliderTop, sliderBottom);
    const int rawFeed = map(constrainedY, sliderBottom, sliderTop, minFeed, maxFeedLimit);
    const int steppedFeed = constrain(((rawFeed + 5) / 10) * 10, minFeed, maxFeedLimit);
    if (steppedFeed != maxFeed) {
        maxFeed = steppedFeed;
        drawSpeedSlider();
    }
}

void initializeColors() {
    colorBackground = display.color565(17, 19, 21);
    colorSurface = display.color565(27, 30, 33);
    colorRaised = display.color565(40, 45, 49);
    colorPad = display.color565(32, 36, 40);
    colorLine = display.color565(56, 62, 67);
    colorMuted = display.color565(155, 164, 170);
    colorText = display.color565(242, 244, 245);
    colorCyan = display.color565(112, 199, 220);
    colorCyanDark = display.color565(39, 79, 89);
    colorGreen = display.color565(74, 181, 126);
    colorAmber = display.color565(224, 169, 70);
    colorRed = display.color565(211, 82, 82);
}

void drawApp() {
    display.setRotation(1);
    initializeColors();
    display.fillScreen(colorBackground);
    drawRail();
    drawPadFrame();
    drawSpeedSlider();
    drawHandle();
    drawConnectionIndicator(true);
}
}  // namespace

void setup() {
    Serial.begin(115200, SERIAL_8N1, 3, 1);
    controllerSerial.begin(115200, SERIAL_8N1, 16, 17);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    display.init();
    drawApp();

    pinMode(touchReset, OUTPUT);
    digitalWrite(touchReset, LOW);
    delay(20);
    digitalWrite(touchReset, HIGH);
    delay(100);
    Wire.begin(touchSda, touchScl, 400000);
}

void loop() {
    static TouchMode touchMode = TouchMode::None;
    TouchPoint point{};
    const bool touched = readTouch(point);
    const int16_t screenX = static_cast<int16_t>(point.y);
    const int16_t screenY = static_cast<int16_t>(320 - point.x);

    if (touched) {
        if (touchMode == TouchMode::None) {
            if (insidePad(screenX, screenY)) {
                touchMode = TouchMode::Pad;
            } else if (insideSpeedSlider(screenX, screenY)) {
                touchMode = TouchMode::Speed;
            }
        }
        if (touchMode == TouchMode::Pad) {
            updatePad(screenX, screenY);
            if (!jogCommandActive) {
                sendJogSegment();
            }
        } else if (touchMode == TouchMode::Speed) {
            updateSpeed(screenY);
        }
    } else if (touchMode != TouchMode::None) {
        if (touchMode == TouchMode::Pad) {
            cancelJog();
            releasePad();
        }
        touchMode = TouchMode::None;
    }

    serviceController();
    delay(2);
}
