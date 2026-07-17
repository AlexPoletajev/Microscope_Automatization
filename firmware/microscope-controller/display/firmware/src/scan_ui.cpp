#include "scan_ui.h"

#include <Preferences.h>

#include <cmath>

namespace {
constexpr int16_t contentLeft = 52;
constexpr int16_t tabTop = 8;
constexpr int16_t tabHeight = 34;
constexpr int16_t tabWidth = 132;
constexpr int16_t compactCenterX = 153;
constexpr int16_t compactCenterY = 166;
constexpr int16_t compactButton = 42;
constexpr int16_t compactOffset = 47;
constexpr int16_t sliderLeft = 205;
constexpr int16_t sliderRight = 455;
constexpr float controllerStepsPerUnit = 160.0F;
constexpr uint32_t cameraPulseMs = 50;
constexpr uint8_t jogCancel = 0x85;
constexpr uint8_t softReset = 0x18;

uint16_t background;
uint16_t surface;
uint16_t raised;
uint16_t line;
uint16_t muted;
uint16_t text;
uint16_t cyan;
uint16_t cyanDark;
uint16_t green;
uint16_t amber;
uint16_t red;

bool inside(int16_t x, int16_t y, int16_t left, int16_t top, int16_t width, int16_t height) {
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

const char* phaseLabel(int phase) {
    switch (phase) {
        case 1: return "POSITIONING";
        case 2: return "MOVING";
        case 3: return "SETTLING";
        case 4: return "TRIGGER";
        case 5: return "CAPTURE";
        case 6: return "PAUSED";
        case 7: return "RETURN";
        case 8: return "COMPLETE";
        case 9: return "ERROR";
        default: return "READY";
    }
}
}  // namespace

ScanUi::ScanUi(TFT_eSPI& display, HardwareSerial& controller)
    : display_(display), controller_(controller) {}

void ScanUi::begin() {
    background = display_.color565(17, 19, 21);
    surface = display_.color565(27, 30, 33);
    raised = display_.color565(40, 45, 49);
    line = display_.color565(56, 62, 67);
    muted = display_.color565(155, 164, 170);
    text = display_.color565(242, 244, 245);
    cyan = display_.color565(112, 199, 220);
    cyanDark = display_.color565(39, 79, 89);
    green = display_.color565(74, 181, 126);
    amber = display_.color565(224, 169, 70);
    red = display_.color565(211, 82, 82);
    loadProfile();
    calculateGrid();
}

void ScanUi::loadProfile() {
    Preferences preferences;
    if (!preferences.begin("scan", true)) return;
    profile_.frameX = preferences.getFloat("frameX", 0.0F);
    profile_.frameY = preferences.getFloat("frameY", 0.0F);
    profile_.corner1X = preferences.getFloat("c1x", 0.0F);
    profile_.corner1Y = preferences.getFloat("c1y", 0.0F);
    profile_.corner2X = preferences.getFloat("c2x", 0.0F);
    profile_.corner2Y = preferences.getFloat("c2y", 0.0F);
    profile_.cameraWidth = preferences.getInt("camW", 1920);
    profile_.cameraHeight = preferences.getInt("camH", 1080);
    profile_.overlap = preferences.getInt("overlap", 15);
    profile_.speed = preferences.getInt("speed", 60);
    profile_.settleMs = preferences.getInt("settle", 300);
    profile_.frameXSet = preferences.getBool("frameXok", false);
    profile_.frameYSet = preferences.getBool("frameYok", false);
    profile_.corner1Set = preferences.getBool("c1ok", false);
    profile_.corner2Set = preferences.getBool("c2ok", false);
    profile_.triggerEnabled = preferences.getBool("trigger", false);
    profile_.returnToStart = preferences.getBool("return", true);
    preferences.end();
}

void ScanUi::saveProfile() {
    Preferences preferences;
    if (!preferences.begin("scan", false)) return;
    preferences.putFloat("frameX", profile_.frameX);
    preferences.putFloat("frameY", profile_.frameY);
    preferences.putFloat("c1x", profile_.corner1X);
    preferences.putFloat("c1y", profile_.corner1Y);
    preferences.putFloat("c2x", profile_.corner2X);
    preferences.putFloat("c2y", profile_.corner2Y);
    preferences.putInt("camW", profile_.cameraWidth);
    preferences.putInt("camH", profile_.cameraHeight);
    preferences.putInt("overlap", profile_.overlap);
    preferences.putInt("speed", profile_.speed);
    preferences.putInt("settle", profile_.settleMs);
    preferences.putBool("frameXok", profile_.frameXSet);
    preferences.putBool("frameYok", profile_.frameYSet);
    preferences.putBool("c1ok", profile_.corner1Set);
    preferences.putBool("c2ok", profile_.corner2Set);
    preferences.putBool("trigger", profile_.triggerEnabled);
    preferences.putBool("return", profile_.returnToStart);
    preferences.end();
}

void ScanUi::drawButton(int16_t x, int16_t y, int16_t width, int16_t height, const char* label, bool active) {
    display_.fillRoundRect(x, y, width, height, 4, active ? cyanDark : raised);
    display_.drawRoundRect(x, y, width, height, 4, active ? cyan : line);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(active ? text : cyan, active ? cyanDark : raised);
    display_.drawString(label, x + width / 2, y + height / 2, 1);
}

void ScanUi::drawTabs() {
    const char* labels[] = { "FRAME", "AREA", "SCAN" };
    for (int index = 0; index < 3; ++index) {
        const int16_t x = 62 + index * (tabWidth + 5);
        const bool selected = static_cast<int>(view_) == index;
        display_.fillRoundRect(x, tabTop, tabWidth, tabHeight, 4, selected ? raised : background);
        if (selected) display_.fillRect(x + 8, tabTop + tabHeight - 3, tabWidth - 16, 3, cyan);
        display_.setTextDatum(MC_DATUM);
        display_.setTextColor(selected ? text : muted, selected ? raised : background);
        display_.drawString(labels[index], x + tabWidth / 2, tabTop + tabHeight / 2, 1);
    }
}

void ScanUi::drawCompactDpad() {
    const int16_t half = compactButton / 2;
    const int16_t centers[][2] = {
        { compactCenterX - compactOffset, compactCenterY },
        { compactCenterX + compactOffset, compactCenterY },
        { compactCenterX, compactCenterY - compactOffset },
        { compactCenterX, compactCenterY + compactOffset }
    };
    for (const auto& center : centers) {
        display_.fillRoundRect(center[0] - half, center[1] - half, compactButton, compactButton, 4, raised);
        display_.drawRoundRect(center[0] - half, center[1] - half, compactButton, compactButton, 4, line);
    }
    display_.fillTriangle(centers[0][0] - 8, centers[0][1], centers[0][0] + 5, centers[0][1] - 8, centers[0][0] + 5, centers[0][1] + 8, cyan);
    display_.fillTriangle(centers[1][0] + 8, centers[1][1], centers[1][0] - 5, centers[1][1] - 8, centers[1][0] - 5, centers[1][1] + 8, cyan);
    display_.fillTriangle(centers[2][0], centers[2][1] - 8, centers[2][0] - 8, centers[2][1] + 5, centers[2][0] + 8, centers[2][1] + 5, cyan);
    display_.fillTriangle(centers[3][0], centers[3][1] + 8, centers[3][0] - 8, centers[3][1] - 5, centers[3][0] + 8, centers[3][1] - 5, cyan);
    display_.fillCircle(compactCenterX, compactCenterY, 18, surface);
    display_.drawCircle(compactCenterX, compactCenterY, 18, line);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(text, surface);
    display_.drawString("XY", compactCenterX, compactCenterY, 1);
    display_.setTextDatum(TC_DATUM);
    display_.setTextColor(muted, background);
    display_.drawString("60 mm/min", compactCenterX, 255, 1);
}

void ScanUi::drawPositionValue(int16_t x, int16_t y, const char* label, float value, bool valid) {
    display_.setTextDatum(TL_DATUM);
    display_.setTextColor(muted, background);
    display_.drawString(label, x, y, 1);
    display_.setTextDatum(TR_DATUM);
    display_.setTextColor(valid ? text : amber, background);
    if (valid) {
        display_.drawString(String(lroundf(value * controllerStepsPerUnit)) + " st", x + 170, y, 1);
    } else {
        display_.drawString("NOT SET", x + 170, y, 1);
    }
}

void ScanUi::drawFrameView() {
    drawCompactDpad();
    display_.drawFastVLine(258, 56, 205, line);
    display_.setTextDatum(TL_DATUM);
    display_.setTextColor(text, background);
    display_.drawString("FIELD WIDTH", 276, 60, 1);
    drawButton(276, 80, 76, 34, "SET A", profile_.frameXASet);
    drawButton(362, 80, 76, 34, "SET B");
    drawPositionValue(276, 123, "X", profile_.frameX, profile_.frameXSet);

    display_.setTextColor(text, background);
    display_.drawString("FIELD HEIGHT", 276, 158, 1);
    drawButton(276, 178, 76, 34, "SET A", profile_.frameYASet);
    drawButton(362, 178, 76, 34, "SET B");
    drawPositionValue(276, 221, "Y", profile_.frameY, profile_.frameYSet);

    display_.drawFastHLine(70, 274, 392, line);
    display_.setTextDatum(TL_DATUM);
    display_.setTextColor(muted, background);
    display_.drawString("CAMERA", 70, 286, 1);
    drawButton(122, 278, 150, 27, (String(profile_.cameraWidth) + " x " + profile_.cameraHeight + " px").c_str());
    if (profile_.frameXSet && profile_.frameYSet) {
        const float sx = profile_.frameX * controllerStepsPerUnit / profile_.cameraWidth;
        const float sy = profile_.frameY * controllerStepsPerUnit / profile_.cameraHeight;
        display_.setTextDatum(TR_DATUM);
        display_.setTextColor(cyan, background);
        display_.drawString(String(sx, 3) + " / " + String(sy, 3) + " st/px", 462, 286, 1);
    }
}

void ScanUi::drawAreaView() {
    drawCompactDpad();
    display_.drawFastVLine(258, 56, 205, line);
    display_.setTextDatum(TL_DATUM);
    display_.setTextColor(text, background);
    display_.drawString("CORNER 1", 276, 60, 1);
    drawButton(276, 80, 76, 34, "SET");
    drawButton(362, 80, 76, 34, "GO");
    drawPositionValue(276, 122, "X", profile_.corner1X, profile_.corner1Set);
    drawPositionValue(276, 139, "Y", profile_.corner1Y, profile_.corner1Set);

    display_.setTextColor(text, background);
    display_.drawString("CORNER 2", 276, 169, 1);
    drawButton(276, 189, 76, 34, "SET");
    drawButton(362, 189, 76, 34, "GO");
    drawPositionValue(276, 231, "X", profile_.corner2X, profile_.corner2Set);
    drawPositionValue(276, 248, "Y", profile_.corner2Y, profile_.corner2Set);

    display_.drawFastHLine(70, 280, 392, line);
    display_.setTextDatum(TL_DATUM);
    display_.setTextColor(muted, background);
    display_.drawString("RANGE", 70, 293, 1);
    display_.setTextColor(profile_.corner1Set && profile_.corner2Set ? cyan : amber, background);
    if (profile_.corner1Set && profile_.corner2Set) {
        display_.drawString(String(lroundf(fabsf(profile_.corner2X - profile_.corner1X) * controllerStepsPerUnit)) + " x " +
                            String(lroundf(fabsf(profile_.corner2Y - profile_.corner1Y) * controllerStepsPerUnit)) + " st", 125, 293, 1);
    } else {
        display_.drawString("SET BOTH CORNERS", 125, 293, 1);
    }
}

void ScanUi::drawSlider(int16_t y, const char* label, int value, int minimum, int maximum, const char* unit) {
    display_.setTextDatum(TL_DATUM);
    display_.setTextColor(muted, background);
    display_.drawString(label, 70, y - 8, 1);
    display_.setTextDatum(TR_DATUM);
    display_.setTextColor(text, background);
    display_.drawString(String(value) + unit, 188, y - 8, 1);
    display_.fillRoundRect(sliderLeft, y - 3, sliderRight - sliderLeft, 6, 3, line);
    const int16_t handle = map(value, minimum, maximum, sliderLeft, sliderRight);
    if (handle > sliderLeft) display_.fillRoundRect(sliderLeft, y - 3, handle - sliderLeft, 6, 3, cyan);
    display_.fillCircle(handle, y, 9, cyan);
    display_.fillCircle(handle, y, 4, text);
}

void ScanUi::drawRunView() {
    if (phase_ != Phase::Idle) {
        drawProgress();
        return;
    }
    calculateGrid();
    drawSlider(72, "OVERLAP", profile_.overlap, 0, 80, "%");
    drawSlider(122, "SPEED", profile_.speed, 1, 90, " mm/min");
    drawSlider(172, "SETTLE", profile_.settleMs, 0, 2000, " ms");
    display_.setTextDatum(TL_DATUM);
    display_.setTextColor(muted, background);
    display_.drawString("CAMERA", 70, 214, 1);
    display_.setTextDatum(TR_DATUM);
    display_.setTextColor(profile_.triggerEnabled ? green : muted, background);
    display_.drawString("IO38 / 50 ms", 455, 214, 1);

    display_.setTextDatum(TL_DATUM);
    display_.setTextColor(profile_.frameXSet && profile_.frameYSet && profile_.corner1Set && profile_.corner2Set ? text : amber, background);
    display_.drawString(String(columns_) + " x " + rows_ + "  /  " + totalImages_ + " images", 70, 252, 1);
    drawButton(70, 274, 170, 36, "START", false);
    drawButton(250, 274, 98, 36, profile_.triggerEnabled ? "CAM ON" : "CAM OFF", profile_.triggerEnabled);
    drawButton(358, 274, 104, 36, profile_.returnToStart ? "RETURN" : "STAY", profile_.returnToStart);
}

void ScanUi::drawProgress() {
    display_.fillRect(contentLeft, 48, 480 - contentLeft, 272, background);
    const int completed = min(currentIndex_, totalImages_);
    display_.setTextDatum(TC_DATUM);
    display_.setTextColor(phase_ == Phase::Error ? red : cyan, background);
    display_.drawString(phaseLabel(static_cast<int>(phase_)), 266, 68, 2);
    display_.setTextColor(text, background);
    display_.drawString(String(completed) + " / " + totalImages_, 266, 102, 4);

    display_.fillRoundRect(78, 148, 376, 12, 6, line);
    const int16_t progressWidth = totalImages_ > 0 ? 376 * completed / totalImages_ : 0;
    if (progressWidth > 0) display_.fillRoundRect(78, 148, progressWidth, 12, 6, phase_ == Phase::Done ? green : cyan);
    display_.setTextColor(muted, background);
    const int row = columns_ > 0 ? min(rows_, currentIndex_ / columns_ + 1) : 0;
    const int column = columns_ > 0 ? min(columns_, currentIndex_ % columns_ + 1) : 0;
    display_.drawString("ROW " + String(row) + "   COLUMN " + String(column), 266, 178, 1);

    if (phase_ == Phase::Done || phase_ == Phase::Error) {
        drawButton(156, 252, 220, 42, "CLOSE", phase_ == Phase::Done);
    } else {
        drawButton(80, 252, 170, 42, pauseRequested_ || phase_ == Phase::Paused ? "RESUME" : "PAUSE", pauseRequested_ || phase_ == Phase::Paused);
        drawButton(282, 252, 170, 42, "STOP", true);
    }
}

void ScanUi::draw() {
    display_.fillRect(contentLeft, 0, 480 - contentLeft, 320, background);
    drawTabs();
    if (view_ == View::Frame) drawFrameView();
    else if (view_ == View::Area) drawAreaView();
    else drawRunView();
}

void ScanUi::redrawCurrentView() {
    display_.fillRect(contentLeft, 48, 480 - contentLeft, 272, background);
    if (view_ == View::Frame) drawFrameView();
    else if (view_ == View::Area) drawAreaView();
    else drawRunView();
}

void ScanUi::switchView(View view) {
    if (running()) return;
    view_ = view;
    draw();
}

ScanUi::JogDirection ScanUi::jogAt(int16_t x, int16_t y) const {
    if (inside(x, y, compactCenterX - compactOffset - compactButton / 2, compactCenterY - compactButton / 2, compactButton, compactButton)) return JogDirection::XNeg;
    if (inside(x, y, compactCenterX + compactOffset - compactButton / 2, compactCenterY - compactButton / 2, compactButton, compactButton)) return JogDirection::XPos;
    if (inside(x, y, compactCenterX - compactButton / 2, compactCenterY - compactOffset - compactButton / 2, compactButton, compactButton)) return JogDirection::YPos;
    if (inside(x, y, compactCenterX - compactButton / 2, compactCenterY + compactOffset - compactButton / 2, compactButton, compactButton)) return JogDirection::YNeg;
    return JogDirection::None;
}

void ScanUi::startJog(JogDirection direction) {
    char axis = direction == JogDirection::XNeg || direction == JogDirection::XPos ? 'X' : 'Y';
    float sign = 1.0F;
    if (direction == JogDirection::XPos || direction == JogDirection::YPos) sign = -1.0F;
    String command = "$J=G91 G21 ";
    command += axis;
    command += String(1000.0F * sign, 1);
    command += " F60\r";
    controller_.write(reinterpret_cast<const uint8_t*>(command.c_str()), command.length());
    controller_.flush();
    jogDirection_ = direction;
    touchAction_ = TouchAction::Jog;
}

void ScanUi::stopJog() {
    controller_.write(jogCancel);
    jogDirection_ = JogDirection::None;
}

void ScanUi::captureFrameX(const ScanMachineStatus& machine) {
    if (!machine.positionValid) return;
    if (!profile_.frameXASet) {
        profile_.frameXA = machine.x;
        profile_.frameXASet = true;
    } else {
        profile_.frameX = fabsf(machine.x - profile_.frameXA);
        profile_.frameXSet = profile_.frameX > 0.00001F;
        profile_.frameXASet = false;
        saveProfile();
        calculateGrid();
    }
    redrawCurrentView();
}

void ScanUi::captureFrameY(const ScanMachineStatus& machine) {
    if (!machine.positionValid) return;
    if (!profile_.frameYASet) {
        profile_.frameYA = machine.y;
        profile_.frameYASet = true;
    } else {
        profile_.frameY = fabsf(machine.y - profile_.frameYA);
        profile_.frameYSet = profile_.frameY > 0.00001F;
        profile_.frameYASet = false;
        saveProfile();
        calculateGrid();
    }
    redrawCurrentView();
}

void ScanUi::captureCorner(bool first, const ScanMachineStatus& machine) {
    if (!machine.positionValid) return;
    if (first) {
        profile_.corner1X = machine.x;
        profile_.corner1Y = machine.y;
        profile_.corner1Set = true;
    } else {
        profile_.corner2X = machine.x;
        profile_.corner2Y = machine.y;
        profile_.corner2Set = true;
    }
    saveProfile();
    calculateGrid();
    redrawCurrentView();
}

void ScanUi::updateSlider(Slider slider, int16_t x) {
    const int16_t position = constrain(x, sliderLeft, sliderRight);
    if (slider == Slider::Overlap) profile_.overlap = map(position, sliderLeft, sliderRight, 0, 80);
    else if (slider == Slider::Speed) profile_.speed = map(position, sliderLeft, sliderRight, 1, 90);
    else if (slider == Slider::Settle) profile_.settleMs = map(position, sliderLeft, sliderRight, 0, 2000);
    calculateGrid();
    redrawCurrentView();
}

void ScanUi::calculateGrid() {
    if (!(profile_.frameXSet && profile_.frameYSet && profile_.corner1Set && profile_.corner2Set)) {
        columns_ = rows_ = totalImages_ = 0;
        return;
    }
    const float strideX = profile_.frameX * (1.0F - profile_.overlap / 100.0F);
    const float strideY = profile_.frameY * (1.0F - profile_.overlap / 100.0F);
    const float spanX = fabsf(profile_.corner2X - profile_.corner1X);
    const float spanY = fabsf(profile_.corner2Y - profile_.corner1Y);
    columns_ = spanX <= 0.00001F ? 1 : static_cast<int>(ceilf(spanX / max(0.00001F, strideX))) + 1;
    rows_ = spanY <= 0.00001F ? 1 : static_cast<int>(ceilf(spanY / max(0.00001F, strideY))) + 1;
    totalImages_ = columns_ * rows_;
}

bool ScanUi::readyToScan(const ScanMachineStatus& machine) const {
    return machine.connected && machine.motion == ScanMotionState::Idle && machine.positionValid &&
        profile_.frameXSet && profile_.frameYSet && profile_.corner1Set && profile_.corner2Set && totalImages_ > 0;
}

void ScanUi::sendLine(const String& lineValue) {
    String line = lineValue;
    line += '\r';
    controller_.write(reinterpret_cast<const uint8_t*>(line.c_str()), line.length());
    controller_.flush();
}

void ScanUi::sendMove(float x, float y) {
    String command = "G90 G21 G1 X";
    command += String(x, 4);
    command += " Y";
    command += String(y, 4);
    command += " F";
    command += String(profile_.speed);
    sendLine(command);
}

void ScanUi::targetForIndex(int index, float& x, float& y) const {
    const int row = columns_ > 0 ? index / columns_ : 0;
    int column = columns_ > 0 ? index % columns_ : 0;
    if (row % 2 != 0) column = columns_ - 1 - column;
    const float xFraction = columns_ > 1 ? static_cast<float>(column) / (columns_ - 1) : 0.0F;
    const float yFraction = rows_ > 1 ? static_cast<float>(row) / (rows_ - 1) : 0.0F;
    x = profile_.corner1X + (profile_.corner2X - profile_.corner1X) * xFraction;
    y = profile_.corner1Y + (profile_.corner2Y - profile_.corner1Y) * yFraction;
}

void ScanUi::startScan(const ScanMachineStatus& machine) {
    calculateGrid();
    if (!readyToScan(machine)) return;
    startX_ = machine.x;
    startY_ = machine.y;
    currentIndex_ = 0;
    stopRequested_ = false;
    pauseRequested_ = false;
    sendLine("M65 P0");
    triggerOutputActive_ = false;
    phase_ = Phase::SendMove;
    phaseStartedAt_ = millis();
    draw();
}

void ScanUi::stopScan() {
    if (triggerOutputActive_) {
        sendLine("M65 P0");
        delay(20);
        triggerOutputActive_ = false;
    }
    controller_.write('!');
    controller_.write(softReset);
    phase_ = Phase::Error;
    stopRequested_ = false;
    pauseRequested_ = false;
    drawProgress();
}

void ScanUi::togglePause() {
    if (phase_ == Phase::Paused) {
        pauseRequested_ = false;
        phase_ = Phase::SendMove;
        phaseStartedAt_ = millis();
    } else {
        pauseRequested_ = !pauseRequested_;
    }
    drawProgress();
}

void ScanUi::advanceScan(const ScanMachineStatus&) {
    ++currentIndex_;
    if (currentIndex_ >= totalImages_) {
        if (profile_.returnToStart) {
            sendMove(startX_, startY_);
            phase_ = Phase::ReturnStart;
            moveObserved_ = false;
            phaseStartedAt_ = millis();
        } else {
            phase_ = Phase::Done;
        }
    } else if (pauseRequested_) {
        phase_ = Phase::Paused;
    } else {
        phase_ = Phase::SendMove;
    }
    drawProgress();
}

void ScanUi::service(const ScanMachineStatus& machine) {
    if (phase_ == Phase::Idle || phase_ == Phase::Done || phase_ == Phase::Error || phase_ == Phase::Paused) return;
    if (stopRequested_) {
        stopScan();
        return;
    }
    if (!machine.connected || machine.motion == ScanMotionState::Blocked) {
        stopScan();
        return;
    }
    const uint32_t now = millis();
    if (phase_ == Phase::SendMove) {
        float x = 0.0F;
        float y = 0.0F;
        targetForIndex(currentIndex_, x, y);
        sendMove(x, y);
        moveObserved_ = false;
        phase_ = Phase::WaitMove;
        phaseStartedAt_ = now;
        drawProgress();
    } else if (phase_ == Phase::WaitMove) {
        if (machine.motion == ScanMotionState::Moving) moveObserved_ = true;
        if (machine.motion == ScanMotionState::Idle && (moveObserved_ || now - phaseStartedAt_ > 350)) {
            phase_ = Phase::Settle;
            phaseStartedAt_ = now;
            drawProgress();
        }
    } else if (phase_ == Phase::Settle && now - phaseStartedAt_ >= static_cast<uint32_t>(profile_.settleMs)) {
        if (profile_.triggerEnabled) {
            sendLine("M64 P0");
            triggerOutputActive_ = true;
            phase_ = Phase::TriggerOn;
            phaseStartedAt_ = now;
            drawProgress();
        } else {
            advanceScan(machine);
        }
    } else if (phase_ == Phase::TriggerOn && now - phaseStartedAt_ >= cameraPulseMs) {
        sendLine("M65 P0");
        triggerOutputActive_ = false;
        phase_ = Phase::TriggerOff;
        phaseStartedAt_ = now;
        drawProgress();
    } else if (phase_ == Phase::TriggerOff && now - phaseStartedAt_ >= 30) {
        advanceScan(machine);
    } else if (phase_ == Phase::ReturnStart) {
        if (machine.motion == ScanMotionState::Moving) moveObserved_ = true;
        if (machine.motion == ScanMotionState::Idle && (moveObserved_ || now - phaseStartedAt_ > 350)) {
            phase_ = Phase::Done;
            drawProgress();
        }
    }
}

void ScanUi::onPress(int16_t x, int16_t y, const ScanMachineStatus& machine) {
    if (!running() && y >= tabTop && y <= tabTop + tabHeight) {
        if (inside(x, y, 62, tabTop, tabWidth, tabHeight)) switchView(View::Frame);
        else if (inside(x, y, 199, tabTop, tabWidth, tabHeight)) switchView(View::Area);
        else if (inside(x, y, 336, tabTop, tabWidth, tabHeight)) switchView(View::Run);
        return;
    }

    if (view_ == View::Run && phase_ != Phase::Idle) {
        if ((phase_ == Phase::Done || phase_ == Phase::Error) && inside(x, y, 156, 252, 220, 42)) {
            phase_ = Phase::Idle;
            draw();
        } else if (phase_ != Phase::Done && phase_ != Phase::Error) {
            if (inside(x, y, 80, 252, 170, 42)) togglePause();
            else if (inside(x, y, 282, 252, 170, 42)) stopRequested_ = true;
        }
        return;
    }

    if (view_ == View::Frame || view_ == View::Area) {
        const JogDirection direction = jogAt(x, y);
        if (direction != JogDirection::None && machine.connected) {
            startJog(direction);
            return;
        }
    }

    if (view_ == View::Frame) {
        if (inside(x, y, 276, 80, 76, 34)) {
            if (machine.positionValid) {
                profile_.frameXA = machine.x;
                profile_.frameXASet = true;
                redrawCurrentView();
            }
        } else if (inside(x, y, 362, 80, 76, 34)) captureFrameX(machine);
        else if (inside(x, y, 276, 178, 76, 34)) {
            if (machine.positionValid) {
                profile_.frameYA = machine.y;
                profile_.frameYASet = true;
                redrawCurrentView();
            }
        } else if (inside(x, y, 362, 178, 76, 34)) captureFrameY(machine);
        else if (inside(x, y, 70, 276, 210, 36)) {
            if (profile_.cameraWidth == 1920) { profile_.cameraWidth = 3840; profile_.cameraHeight = 2160; }
            else if (profile_.cameraWidth == 3840) { profile_.cameraWidth = 5472; profile_.cameraHeight = 3648; }
            else { profile_.cameraWidth = 1920; profile_.cameraHeight = 1080; }
            saveProfile();
            redrawCurrentView();
        }
    } else if (view_ == View::Area) {
        if (inside(x, y, 276, 80, 76, 34)) captureCorner(true, machine);
        else if (inside(x, y, 362, 80, 76, 34) && profile_.corner1Set && machine.motion == ScanMotionState::Idle) sendMove(profile_.corner1X, profile_.corner1Y);
        else if (inside(x, y, 276, 189, 76, 34)) captureCorner(false, machine);
        else if (inside(x, y, 362, 189, 76, 34) && profile_.corner2Set && machine.motion == ScanMotionState::Idle) sendMove(profile_.corner2X, profile_.corner2Y);
    } else {
        if (y >= 55 && y <= 90) activeSlider_ = Slider::Overlap;
        else if (y >= 105 && y <= 140) activeSlider_ = Slider::Speed;
        else if (y >= 155 && y <= 190) activeSlider_ = Slider::Settle;
        if (activeSlider_ != Slider::None && x >= 190) {
            touchAction_ = TouchAction::Slider;
            updateSlider(activeSlider_, x);
        } else if (inside(x, y, 70, 274, 170, 36)) startScan(machine);
        else if (inside(x, y, 250, 274, 98, 36)) {
            profile_.triggerEnabled = !profile_.triggerEnabled;
            saveProfile();
            redrawCurrentView();
        } else if (inside(x, y, 358, 274, 104, 36)) {
            profile_.returnToStart = !profile_.returnToStart;
            saveProfile();
            redrawCurrentView();
        }
    }
}

void ScanUi::onDrag(int16_t x, int16_t) {
    if (touchAction_ == TouchAction::Slider) updateSlider(activeSlider_, x);
}

void ScanUi::onRelease() {
    if (touchAction_ == TouchAction::Jog) stopJog();
    if (touchAction_ == TouchAction::Slider) saveProfile();
    touchAction_ = TouchAction::None;
    activeSlider_ = Slider::None;
}

void ScanUi::handleTouch(bool touched, int16_t x, int16_t y, const ScanMachineStatus& machine) {
    if (touched && !touchWasDown_) onPress(x, y, machine);
    else if (touched && touchWasDown_) onDrag(x, y);
    else if (!touched && touchWasDown_) onRelease();
    touchWasDown_ = touched;
}

bool ScanUi::running() const {
    return phase_ != Phase::Idle && phase_ != Phase::Done && phase_ != Phase::Error;
}
