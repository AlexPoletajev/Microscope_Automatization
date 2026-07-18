#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>

#include "scan_ui.h"

#include <cmath>
#include <cstring>

namespace {
TFT_eSPI display;
HardwareSerial controllerSerial(2);
ScanUi scanUi(display, controllerSerial);

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
constexpr int16_t dpadLeftCenterX = 160;
constexpr int16_t dpadRightCenterX = 372;
constexpr int16_t dpadCenterY = 150;
constexpr int16_t dpadButtonSize = 58;
constexpr int16_t dpadOffset = 64;
constexpr int16_t axisSliderLeft = 76;
constexpr int16_t axisSliderRight = 460;
constexpr int16_t axisSliderY = 286;
constexpr float deadzone = 0.12F;
constexpr int minFeed = 30;
constexpr int maxFeedLimit = 600;
constexpr int axisPageMinFeed = 1;
constexpr int axisPageMaxFeed = 180;
constexpr uint32_t statusRequestIntervalMs = 250;
constexpr uint32_t statusTimeoutMs = 1000;
constexpr uint32_t jogIntervalMs = 110;
constexpr uint32_t jogHorizonMs = 190;
constexpr uint32_t navigationDoubleTapMs = 500;
constexpr float axisJogDistance = 1000.0F;
constexpr uint8_t jogCancel = 0x85;

int maxFeed = 60;
int axisFeed = 60;

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

enum class Page { Pad, Axes, Scan, Settings };
enum class TouchMode { None, Pad, Speed, AxisSpeed, AxisJog, Navigation };
enum class AxisDirection { None, XNeg, XPos, YNeg, YPos, ZNeg, ZPos, ANeg, APos };
enum class MachineState { Unknown, Idle, Jog, Moving, Blocked };
enum class ConnectionState { Offline, Ready, Blocked };

Page currentPage = Page::Pad;
Page pendingNavigationPage = Page::Pad;
uint32_t pendingNavigationAt = 0;
bool navigationTapPending = false;
PadState padState;
AxisDirection activeAxisDirection = AxisDirection::None;
MachineState machineState = MachineState::Unknown;
ConnectionState drawnConnectionState = ConnectionState::Offline;
uint32_t lastStatusAt = 0;
uint32_t lastStatusRequestAt = 0;
uint32_t lastJogAt = 0;
bool receivedStatus = false;
bool jogCommandActive = false;
float workPositionX = 0.0F;
float workPositionY = 0.0F;
float workPositionZ = 0.0F;
float workOffsetX = 0.0F;
float workOffsetY = 0.0F;
float workOffsetZ = 0.0F;
bool positionValid = false;
char statusFrame[160];
size_t statusFrameLength = 0;
bool receivingStatusFrame = false;

void drawUiText(const String& value, int16_t x, int16_t y, bool large = false) {
    display.setFreeFont(large ? &FreeSans12pt7b : &FreeSans9pt7b);
    display.drawString(value, x, y);
    display.setTextFont(1);
}

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
        display.fillRect(centerX - 10, centerY + offset - 1, 20, 3, color);
    }
}

void drawControlIcon(int16_t centerX, int16_t centerY, uint16_t color) {
    display.drawCircle(centerX, centerY, 9, color);
    display.drawCircle(centerX, centerY, 10, color);
    display.fillRect(centerX - 13, centerY - 1, 26, 3, color);
    display.fillRect(centerX - 1, centerY - 13, 3, 26, color);
    display.fillCircle(centerX, centerY, 3, color);
}

void drawScanIcon(int16_t centerX, int16_t centerY, uint16_t color) {
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 2; ++column) {
            display.drawRect(centerX - 10 + column * 12, centerY - 10 + row * 12, 8, 8, color);
            display.drawRect(centerX - 9 + column * 12, centerY - 9 + row * 12, 6, 6, color);
        }
    }
}

void drawAxesIcon(int16_t centerX, int16_t centerY, uint16_t color) {
    display.fillRect(centerX - 11, centerY - 1, 22, 3, color);
    display.fillRect(centerX - 1, centerY - 11, 3, 22, color);
    display.fillTriangle(centerX - 12, centerY, centerX - 6, centerY - 4, centerX - 6, centerY + 4, color);
    display.fillTriangle(centerX + 12, centerY, centerX + 6, centerY - 4, centerX + 6, centerY + 4, color);
    display.fillTriangle(centerX, centerY - 12, centerX - 4, centerY - 6, centerX + 4, centerY - 6, color);
    display.fillTriangle(centerX, centerY + 12, centerX - 4, centerY + 6, centerX + 4, centerY + 6, color);
}

void drawSettingsIcon(int16_t centerX, int16_t centerY, uint16_t color) {
    display.fillRect(centerX - 11, centerY - 8, 22, 3, color);
    display.fillRect(centerX - 11, centerY - 1, 22, 3, color);
    display.fillRect(centerX - 11, centerY + 6, 22, 3, color);
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
        display.drawRoundRect(7, y, 38, 38, 4, colorCyan);
        display.drawRoundRect(8, y + 1, 36, 36, 3, colorCyan);
        display.fillRect(0, y + 6, 5, 26, colorCyan);
    }
    drawIcon(26, y + 19, selected ? colorCyan : colorMuted);
}

void drawRail() {
    display.fillRect(0, 0, railWidth, 320, colorSurface);
    display.fillRect(railWidth - 2, 0, 2, 320, colorLine);
    drawRailButton(8, false, drawMenuIcon);
    drawRailButton(68, currentPage == Page::Pad, drawControlIcon);
    drawRailButton(120, currentPage == Page::Axes, drawAxesIcon);
    drawRailButton(172, currentPage == Page::Scan, drawScanIcon);
    drawRailButton(274, currentPage == Page::Settings, drawSettingsIcon);
}

ConnectionState connectionState() {
    if (!receivedStatus || millis() - lastStatusAt > statusTimeoutMs) {
        return ConnectionState::Offline;
    }
    return machineState == MachineState::Idle || machineState == MachineState::Jog
        ? ConnectionState::Ready
        : ConnectionState::Blocked;
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
        if (index == 5) {
            display.fillRect(padX + offset - 1, padY + 1, 2, padSize - 2, lineColor);
            display.fillRect(padX + 1, padY + offset - 1, padSize - 2, 2, lineColor);
        } else {
            display.drawFastVLine(padX + offset, padY + 1, padSize - 2, lineColor);
            display.drawFastHLine(padX + 1, padY + offset, padSize - 2, lineColor);
        }
    }
    display.drawCircle(padCenterX, padCenterY, static_cast<int16_t>(padSize * deadzone), colorMuted);
    display.fillCircle(padCenterX, padCenterY, 2, colorMuted);
}

void drawPadFrame() {
    display.fillRoundRect(padX, padY, padSize, padSize, 5, colorPad);
    display.drawRoundRect(padX, padY, padSize, padSize, 5, colorLine);
    display.drawRoundRect(padX + 1, padY + 1, padSize - 2, padSize - 2, 4, colorLine);
    drawPadGrid();

    display.setTextColor(colorMuted, colorPad);
    display.setTextDatum(TC_DATUM);
    drawUiText("Y+", padCenterX, padY + 8);
    display.setTextDatum(MR_DATUM);
    drawUiText("X+", padX + padSize - 8, padCenterY);
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
    display.fillRect(sliderPanelX, 0, 2, 320, colorLine);

    display.setTextDatum(TC_DATUM);
    display.setTextColor(colorText, colorSurface);
    drawUiText("TEMPO", sliderCenterX, 14, true);
    display.setTextColor(colorMuted, colorSurface);
    drawUiText(String(maxFeedLimit), sliderCenterX, sliderTop - 18);
    drawUiText(String(minFeed), sliderCenterX, sliderBottom + 10);

    display.fillRoundRect(sliderCenterX - 3, sliderTop, 6, sliderBottom - sliderTop, 3, colorLine);
    const int16_t knobY = map(maxFeed, minFeed, maxFeedLimit, sliderBottom, sliderTop);
    display.fillRoundRect(sliderCenterX - 3, knobY, 6, sliderBottom - knobY, 3, colorCyan);
    for (int index = 0; index <= 6; ++index) {
        const int16_t y = sliderTop + index * (sliderBottom - sliderTop) / 6;
        display.fillRect(sliderCenterX - 18, y - 1, 9, 3, colorMuted);
        display.fillRect(sliderCenterX + 10, y - 1, 9, 3, colorMuted);
    }
    display.fillCircle(sliderCenterX, knobY, 14, colorRaised);
    display.drawCircle(sliderCenterX, knobY, 14, colorText);
    display.drawCircle(sliderCenterX, knobY, 13, colorCyan);

    display.fillRect(sliderPanelX + 8, 278, 92, 34, colorSurface);
    display.setTextDatum(TC_DATUM);
    display.setTextColor(colorText, colorSurface);
    drawUiText(String(maxFeed), sliderCenterX, 278, true);
    display.setTextColor(colorMuted, colorSurface);
    drawUiText("mm/min", sliderCenterX, 296);
}

void drawArrowButton(int16_t centerX, int16_t centerY, int8_t dx, int8_t dy, bool pressed = false) {
    const int16_t half = dpadButtonSize / 2;
    const uint16_t fill = pressed ? colorCyanDark : colorRaised;
    const uint16_t arrow = pressed ? colorText : colorCyan;
    display.fillRoundRect(centerX - half, centerY - half, dpadButtonSize, dpadButtonSize, 5, fill);
    display.drawRoundRect(centerX - half, centerY - half, dpadButtonSize, dpadButtonSize, 5, pressed ? colorCyan : colorLine);
    display.drawRoundRect(centerX - half + 1, centerY - half + 1, dpadButtonSize - 2, dpadButtonSize - 2, 4, pressed ? colorCyan : colorLine);

    if (dx < 0) {
        display.fillTriangle(centerX - 11, centerY, centerX + 7, centerY - 11, centerX + 7, centerY + 11, arrow);
    } else if (dx > 0) {
        display.fillTriangle(centerX + 11, centerY, centerX - 7, centerY - 11, centerX - 7, centerY + 11, arrow);
    } else if (dy < 0) {
        display.fillTriangle(centerX, centerY - 11, centerX - 11, centerY + 7, centerX + 11, centerY + 7, arrow);
    } else {
        display.fillTriangle(centerX, centerY + 11, centerX - 11, centerY - 7, centerX + 11, centerY - 7, arrow);
    }
}

void drawDpad(int16_t centerX, const char* title, const char* horizontalAxis, const char* verticalAxis) {
    display.setTextDatum(TC_DATUM);
    display.setTextColor(colorText, colorBackground);
    drawUiText(title, centerX, 12, true);
    display.setTextColor(colorMuted, colorBackground);
    drawUiText(horizontalAxis, centerX, 34);
    drawUiText(verticalAxis, centerX + 25, dpadCenterY - 6);

    drawArrowButton(centerX - dpadOffset, dpadCenterY, -1, 0);
    drawArrowButton(centerX + dpadOffset, dpadCenterY, 1, 0);
    drawArrowButton(centerX, dpadCenterY - dpadOffset, 0, -1);
    drawArrowButton(centerX, dpadCenterY + dpadOffset, 0, 1);

    display.fillCircle(centerX, dpadCenterY, 24, colorPad);
    display.drawCircle(centerX, dpadCenterY, 24, colorLine);
    display.drawCircle(centerX, dpadCenterY, 23, colorLine);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(colorText, colorPad);
    drawUiText(title, centerX, dpadCenterY, true);
}

void drawAxisSpeedControl() {
    display.fillRect(railWidth, 250, 480 - railWidth, 70, colorBackground);
    const int16_t handleX = map(axisFeed, axisPageMinFeed, axisPageMaxFeed, axisSliderLeft, axisSliderRight);
    display.fillRoundRect(axisSliderLeft, axisSliderY - 5, axisSliderRight - axisSliderLeft, 10, 5, colorLine);
    if (handleX > axisSliderLeft) {
        display.fillRoundRect(axisSliderLeft, axisSliderY - 5, handleX - axisSliderLeft, 10, 5, colorCyan);
    }
    display.fillCircle(handleX, axisSliderY, 11, colorCyan);
    display.fillCircle(handleX, axisSliderY, 5, colorText);

    display.setTextDatum(TC_DATUM);
    display.setTextColor(colorText, colorBackground);
    drawUiText(String(axisFeed), (axisSliderLeft + axisSliderRight) / 2, 252, true);
    display.setTextColor(colorMuted, colorBackground);
    drawUiText("mm/min", (axisSliderLeft + axisSliderRight) / 2, 268);
    display.setTextDatum(TL_DATUM);
    drawUiText(String(axisPageMinFeed), axisSliderLeft, 300);
    display.setTextDatum(TR_DATUM);
    drawUiText(String(axisPageMaxFeed), axisSliderRight, 300);
}

void drawAxesPage() {
    display.fillRect(railWidth, 0, 480 - railWidth, 320, colorBackground);
    display.fillRect(265, 48, 2, 202, colorLine);
    drawDpad(dpadLeftCenterX, "XY", "X", "Y");
    drawDpad(dpadRightCenterX, "ZA", "Z", "A");
    drawAxisSpeedControl();
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
    padState.vectorX = scaledMagnitude > 0.0F ? -x : 0.0F;
    padState.vectorY = scaledMagnitude > 0.0F ? -y : 0.0F;
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

bool axisDirectionCommand(AxisDirection direction, char& axis, float& sign) {
    switch (direction) {
        case AxisDirection::XNeg: axis = 'X'; sign = 1.0F; return true;
        case AxisDirection::XPos: axis = 'X'; sign = -1.0F; return true;
        case AxisDirection::YNeg: axis = 'Y'; sign = 1.0F; return true;
        case AxisDirection::YPos: axis = 'Y'; sign = -1.0F; return true;
        case AxisDirection::ZNeg: axis = 'Z'; sign = -1.0F; return true;
        case AxisDirection::ZPos: axis = 'Z'; sign = 1.0F; return true;
        case AxisDirection::ANeg: axis = 'A'; sign = -1.0F; return true;
        case AxisDirection::APos: axis = 'A'; sign = 1.0F; return true;
        case AxisDirection::None: return false;
    }
    return false;
}

void sendAxisJogSegment() {
    char axis = '\0';
    float sign = 0.0F;
    if (!axisDirectionCommand(activeAxisDirection, axis, sign) || connectionState() != ConnectionState::Ready) {
        if (jogCommandActive) {
            cancelJog();
        }
        return;
    }

    const float feed = static_cast<float>(axisFeed);
    String command;
    command.reserve(48);
    command = F("$J=G91 G21 ");
    command += axis;
    command += String(axisJogDistance * sign, 1);
    command += F(" F");
    command += String(feed, 1);
    command += '\r';
    controllerSerial.write(reinterpret_cast<const uint8_t*>(command.c_str()), command.length());
    controllerSerial.flush();
    jogCommandActive = true;
    lastJogAt = millis();
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
    char* save = nullptr;
    char* field = strtok_r(statusFrame, "|", &save);
    if (field == nullptr) return;
    if (strcmp(field, "Idle") == 0) machineState = MachineState::Idle;
    else if (strcmp(field, "Jog") == 0) machineState = MachineState::Jog;
    else if (strcmp(field, "Run") == 0) machineState = MachineState::Moving;
    else machineState = MachineState::Blocked;

    float machineX = 0.0F;
    float machineY = 0.0F;
    float machineZ = 0.0F;
    bool haveMachinePosition = false;
    while ((field = strtok_r(nullptr, "|", &save)) != nullptr) {
        if (sscanf(field, "MPos:%f,%f,%f", &machineX, &machineY, &machineZ) == 3) {
            haveMachinePosition = true;
        } else if (sscanf(field, "WPos:%f,%f,%f", &workPositionX, &workPositionY, &workPositionZ) == 3) {
            positionValid = true;
        } else if (sscanf(field, "WCO:%f,%f,%f", &workOffsetX, &workOffsetY, &workOffsetZ) == 3) {
            if (haveMachinePosition) {
                workPositionX = machineX - workOffsetX;
                workPositionY = machineY - workOffsetY;
                workPositionZ = machineZ - workOffsetZ;
                positionValid = true;
            }
        }
    }
    if (haveMachinePosition) {
        workPositionX = machineX - workOffsetX;
        workPositionY = machineY - workOffsetY;
        workPositionZ = machineZ - workOffsetZ;
        positionValid = true;
    }
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
        if (statusFrameLength < sizeof(statusFrame) - 1) {
            statusFrame[statusFrameLength++] = incoming;
        } else {
            receivingStatusFrame = false;
            statusFrameLength = 0;
        }
    }
}

ScanMachineStatus scanMachineStatus() {
    ScanMachineStatus status;
    status.connected = receivedStatus && millis() - lastStatusAt <= statusTimeoutMs;
    status.positionValid = positionValid;
    status.x = workPositionX;
    status.y = workPositionY;
    status.z = workPositionZ;
    if (machineState == MachineState::Idle) status.motion = ScanMotionState::Idle;
    else if (machineState == MachineState::Moving || machineState == MachineState::Jog) status.motion = ScanMotionState::Moving;
    else if (machineState == MachineState::Blocked) status.motion = ScanMotionState::Blocked;
    return status;
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
    scanUi.service(scanMachineStatus());
    drawConnectionIndicator();
}

bool insidePad(int16_t x, int16_t y) {
    return x >= padX && x < padX + padSize && y >= padY && y < padY + padSize;
}

bool insideSpeedSlider(int16_t x, int16_t y) {
    return x >= sliderPanelX && y >= sliderTop - 20 && y <= sliderBottom + 20;
}

bool insideSquareButton(int16_t x, int16_t y, int16_t centerX, int16_t centerY, int16_t size) {
    const int16_t half = size / 2;
    return x >= centerX - half && x <= centerX + half && y >= centerY - half && y <= centerY + half;
}

AxisDirection axisDirectionAt(int16_t x, int16_t y) {
    if (insideSquareButton(x, y, dpadLeftCenterX - dpadOffset, dpadCenterY, dpadButtonSize)) return AxisDirection::XNeg;
    if (insideSquareButton(x, y, dpadLeftCenterX + dpadOffset, dpadCenterY, dpadButtonSize)) return AxisDirection::XPos;
    if (insideSquareButton(x, y, dpadLeftCenterX, dpadCenterY - dpadOffset, dpadButtonSize)) return AxisDirection::YPos;
    if (insideSquareButton(x, y, dpadLeftCenterX, dpadCenterY + dpadOffset, dpadButtonSize)) return AxisDirection::YNeg;
    if (insideSquareButton(x, y, dpadRightCenterX - dpadOffset, dpadCenterY, dpadButtonSize)) return AxisDirection::ZNeg;
    if (insideSquareButton(x, y, dpadRightCenterX + dpadOffset, dpadCenterY, dpadButtonSize)) return AxisDirection::ZPos;
    if (insideSquareButton(x, y, dpadRightCenterX, dpadCenterY - dpadOffset, dpadButtonSize)) return AxisDirection::APos;
    if (insideSquareButton(x, y, dpadRightCenterX, dpadCenterY + dpadOffset, dpadButtonSize)) return AxisDirection::ANeg;
    return AxisDirection::None;
}

void drawAxisDirection(AxisDirection direction, bool pressed) {
    switch (direction) {
        case AxisDirection::XNeg: drawArrowButton(dpadLeftCenterX - dpadOffset, dpadCenterY, -1, 0, pressed); break;
        case AxisDirection::XPos: drawArrowButton(dpadLeftCenterX + dpadOffset, dpadCenterY, 1, 0, pressed); break;
        case AxisDirection::YPos: drawArrowButton(dpadLeftCenterX, dpadCenterY - dpadOffset, 0, -1, pressed); break;
        case AxisDirection::YNeg: drawArrowButton(dpadLeftCenterX, dpadCenterY + dpadOffset, 0, 1, pressed); break;
        case AxisDirection::ZNeg: drawArrowButton(dpadRightCenterX - dpadOffset, dpadCenterY, -1, 0, pressed); break;
        case AxisDirection::ZPos: drawArrowButton(dpadRightCenterX + dpadOffset, dpadCenterY, 1, 0, pressed); break;
        case AxisDirection::APos: drawArrowButton(dpadRightCenterX, dpadCenterY - dpadOffset, 0, -1, pressed); break;
        case AxisDirection::ANeg: drawArrowButton(dpadRightCenterX, dpadCenterY + dpadOffset, 0, 1, pressed); break;
        case AxisDirection::None: break;
    }
}

bool insidePadPageButton(int16_t x, int16_t y) {
    return x >= 5 && x <= 47 && y >= 64 && y <= 110;
}

bool insideAxesPageButton(int16_t x, int16_t y) {
    return x >= 5 && x <= 47 && y >= 116 && y <= 166;
}

bool insideScanPageButton(int16_t x, int16_t y) {
    return x >= 5 && x <= 47 && y >= 168 && y <= 218;
}

bool insideSettingsPageButton(int16_t x, int16_t y) {
    return x >= 5 && x <= 47 && y >= 270 && y <= 319;
}

bool insideAxisSpeedSlider(int16_t x, int16_t y) {
    return x >= axisSliderLeft - 12 && x <= axisSliderRight + 12 && y >= 258 && y <= 319;
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

void drawPadPage() {
    display.fillRect(railWidth, 0, 480 - railWidth, 320, colorBackground);
    drawPadFrame();
    drawSpeedSlider();
    drawHandle();
}

void showPage(Page page) {
    if (page == currentPage) {
        return;
    }
    if (scanUi.running()) {
        return;
    }
    if (jogCommandActive) {
        cancelJog();
    }
    scanUi.cancelInteraction();
    padState = PadState{};
    activeAxisDirection = AxisDirection::None;
    currentPage = page;
    drawRail();
    if (currentPage == Page::Pad) {
        drawPadPage();
    } else if (currentPage == Page::Axes) {
        drawAxesPage();
    } else if (currentPage == Page::Scan) {
        scanUi.showWorkflow();
    } else {
        scanUi.showSettings();
    }
    drawConnectionIndicator(true);
}

void requestPage(Page page) {
    if (page == currentPage) {
        navigationTapPending = false;
        return;
    }
    const uint32_t now = millis();
    if (navigationTapPending && pendingNavigationPage == page && now - pendingNavigationAt <= navigationDoubleTapMs) {
        navigationTapPending = false;
        showPage(page);
        return;
    }
    pendingNavigationPage = page;
    pendingNavigationAt = now;
    navigationTapPending = true;
}

void updateAxisSpeed(int16_t screenX) {
    const int16_t constrainedX = constrain(screenX, axisSliderLeft, axisSliderRight);
    axisFeed = map(constrainedX, axisSliderLeft, axisSliderRight, axisPageMinFeed, axisPageMaxFeed);
    drawAxisSpeedControl();
}

void initializeColors() {
    colorBackground = display.color565(4, 6, 8);
    colorSurface = display.color565(22, 27, 31);
    colorRaised = display.color565(55, 65, 72);
    colorPad = display.color565(20, 27, 32);
    colorLine = display.color565(105, 119, 128);
    colorMuted = display.color565(205, 215, 220);
    colorText = display.color565(255, 255, 255);
    colorCyan = display.color565(96, 226, 255);
    colorCyanDark = display.color565(23, 100, 119);
    colorGreen = display.color565(79, 229, 145);
    colorAmber = display.color565(255, 192, 72);
    colorRed = display.color565(255, 91, 91);
}

void drawApp() {
    display.setRotation(1);
    initializeColors();
    display.fillScreen(colorBackground);
    drawRail();
    if (currentPage == Page::Pad) {
        drawPadPage();
    } else if (currentPage == Page::Axes) {
        drawAxesPage();
    } else if (currentPage == Page::Scan) {
        scanUi.showWorkflow();
    } else {
        scanUi.showSettings();
    }
    drawConnectionIndicator(true);
}
}  // namespace

void setup() {
    Serial.begin(115200, SERIAL_8N1, 3, 1);
    controllerSerial.begin(115200, SERIAL_8N1, 16, 17);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    display.init();
    scanUi.begin();
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

    if (currentPage == Page::Scan || currentPage == Page::Settings) {
        bool navigationTouch = false;
        if (touched && touchMode == TouchMode::None) {
            if (insidePadPageButton(screenX, screenY)) {
                requestPage(Page::Pad);
                touchMode = TouchMode::Navigation;
                navigationTouch = true;
            } else if (insideAxesPageButton(screenX, screenY)) {
                requestPage(Page::Axes);
                touchMode = TouchMode::Navigation;
                navigationTouch = true;
            } else if (insideScanPageButton(screenX, screenY)) {
                requestPage(Page::Scan);
                touchMode = TouchMode::Navigation;
                navigationTouch = true;
            } else if (insideSettingsPageButton(screenX, screenY)) {
                requestPage(Page::Settings);
                touchMode = TouchMode::Navigation;
                navigationTouch = true;
            }
        }
        if ((currentPage == Page::Scan || currentPage == Page::Settings) && !navigationTouch && touchMode == TouchMode::None) {
            scanUi.handleTouch(touched, screenX, screenY, scanMachineStatus());
        } else if (!touched && touchMode == TouchMode::Navigation) {
            touchMode = TouchMode::None;
        }
        serviceController();
        delay(2);
        return;
    }

    if (touched) {
        if (touchMode == TouchMode::None) {
            if (insidePadPageButton(screenX, screenY)) {
                requestPage(Page::Pad);
                touchMode = TouchMode::Navigation;
            } else if (insideAxesPageButton(screenX, screenY)) {
                requestPage(Page::Axes);
                touchMode = TouchMode::Navigation;
            } else if (insideScanPageButton(screenX, screenY)) {
                requestPage(Page::Scan);
                touchMode = TouchMode::Navigation;
            } else if (insideSettingsPageButton(screenX, screenY)) {
                requestPage(Page::Settings);
                touchMode = TouchMode::Navigation;
            } else if (currentPage == Page::Pad) {
                if (insidePad(screenX, screenY)) {
                    touchMode = TouchMode::Pad;
                } else if (insideSpeedSlider(screenX, screenY)) {
                    touchMode = TouchMode::Speed;
                }
            } else {
                activeAxisDirection = axisDirectionAt(screenX, screenY);
                if (touchMode == TouchMode::None && activeAxisDirection != AxisDirection::None) {
                    touchMode = TouchMode::AxisJog;
                    drawAxisDirection(activeAxisDirection, true);
                    sendAxisJogSegment();
                } else if (touchMode == TouchMode::None && insideAxisSpeedSlider(screenX, screenY)) {
                    touchMode = TouchMode::AxisSpeed;
                }
            }
        }
        if (touchMode == TouchMode::Pad) {
            updatePad(screenX, screenY);
            if (!jogCommandActive) {
                sendJogSegment();
            }
        } else if (touchMode == TouchMode::Speed) {
            updateSpeed(screenY);
        } else if (touchMode == TouchMode::AxisSpeed) {
            updateAxisSpeed(screenX);
        }
    } else if (touchMode != TouchMode::None) {
        if (touchMode == TouchMode::Pad) {
            cancelJog();
            releasePad();
        } else if (touchMode == TouchMode::AxisJog) {
            cancelJog();
            drawAxisDirection(activeAxisDirection, false);
            activeAxisDirection = AxisDirection::None;
        }
        touchMode = TouchMode::None;
    }

    serviceController();
    delay(2);
}
