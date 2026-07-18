#include "scan_ui.h"

#include <Preferences.h>
#include <cmath>

namespace {
constexpr int16_t left = 52;
constexpr int16_t sliderLeft = 145;
constexpr int16_t sliderRight = 386;
constexpr int scanMaxSpeed = 600;
constexpr int maxFocusSteps = 20;
constexpr uint32_t cameraPulseMs = 50;
constexpr uint8_t jogCancel = 0x85;
constexpr uint8_t softReset = 0x18;

uint16_t background, surface, raised, lineColor, muted, text, cyan, cyanDark, green, amber, red;

void drawUiText(TFT_eSPI& display, const String& value, int16_t x, int16_t y, bool large = false) {
    display.setFreeFont(large ? &FreeSans12pt7b : &FreeSans9pt7b);
    display.drawString(value, x, y);
    display.setTextFont(1);
}

bool inside(int16_t x, int16_t y, int16_t bx, int16_t by, int16_t w, int16_t h) {
    return x >= bx && x <= bx + w && y >= by && y <= by + h;
}

const char* phaseLabel(int value) {
    switch (value) {
        case 1: return "POSITIONIEREN";
        case 2: return "FAHRT";
        case 3: return "RUHEZEIT";
        case 4: return "KAMERA";
        case 5: return "AUFNAHME";
        case 6: return "PAUSE";
        case 7: return "RUECKFAHRT";
        case 8: return "FERTIG";
        case 9: return "ABGEBROCHEN";
        default: return "BEREIT";
    }
}
}  // namespace

ScanUi::ScanUi(TFT_eSPI& display, HardwareSerial& controller) : display_(display), controller_(controller) {}

void ScanUi::begin() {
    background = display_.color565(4, 6, 8);
    surface = display_.color565(22, 27, 31);
    raised = display_.color565(55, 65, 72);
    lineColor = display_.color565(105, 119, 128);
    muted = display_.color565(205, 215, 220);
    text = display_.color565(255, 255, 255);
    cyan = display_.color565(96, 226, 255);
    cyanDark = display_.color565(23, 100, 119);
    green = display_.color565(79, 229, 145);
    amber = display_.color565(255, 192, 72);
    red = display_.color565(255, 91, 91);
    loadProfile();
    migrateLegacyProfile();
    calculateGrid();
}

void ScanUi::loadProfile() {
    Preferences p;
    if (!p.begin("scan", true)) return;
    profile_.frameX = p.getFloat("frameX", 0.0F);
    profile_.frameY = p.getFloat("frameY", 0.0F);
    profile_.cameraWidth = p.getInt("camW", 1920);
    profile_.cameraHeight = p.getInt("camH", 1080);
    profile_.overlap = constrain(p.getInt("overlap", 15), 0, 80);
    profile_.speed = constrain(p.getInt("speed", 60), 1, scanMaxSpeed);
    profile_.settleMs = constrain((p.getInt("settle", 300) + 25) / 50 * 50, 0, 2000);
    profile_.focusSteps = constrain(p.getInt("focusN", 1), 1, maxFocusSteps);
    profile_.frameXSet = p.getBool("frameXok", false) && profile_.frameX > 0.00001F;
    profile_.frameYSet = p.getBool("frameYok", false) && profile_.frameY > 0.00001F;
    profile_.cameraEnabled = p.getBool("camera", p.getBool("trigger", false));
    profile_.returnToStart = p.getBool("return", true);
    p.end();
    const bool knownResolution = (profile_.cameraWidth == 1920 && profile_.cameraHeight == 1080) ||
        (profile_.cameraWidth == 3840 && profile_.cameraHeight == 2160) ||
        (profile_.cameraWidth == 5472 && profile_.cameraHeight == 3648);
    if (!knownResolution) { profile_.cameraWidth = 1920; profile_.cameraHeight = 1080; }
}

void ScanUi::migrateLegacyProfile() {
    Preferences p;
    if (!p.begin("scan", false)) return;
    p.remove("c1x"); p.remove("c1y"); p.remove("c2x"); p.remove("c2y");
    p.remove("c1ok"); p.remove("c2ok"); p.remove("pulse"); p.remove("triggerMs");
    p.remove("frameZ"); p.remove("frameZok");
    p.putBool("camera", profile_.cameraEnabled);
    p.remove("trigger");
    p.end();
    saveProfile();
}

void ScanUi::saveProfile() {
    Preferences p;
    if (!p.begin("scan", false)) return;
    p.putFloat("frameX", profile_.frameX); p.putFloat("frameY", profile_.frameY);
    p.putBool("frameXok", profile_.frameXSet); p.putBool("frameYok", profile_.frameYSet);
    p.putInt("camW", profile_.cameraWidth); p.putInt("camH", profile_.cameraHeight);
    p.putInt("overlap", profile_.overlap); p.putInt("speed", profile_.speed);
    p.putInt("settle", profile_.settleMs); p.putInt("focusN", profile_.focusSteps); p.putBool("camera", profile_.cameraEnabled);
    p.putBool("return", profile_.returnToStart);
    p.end();
}

void ScanUi::showWorkflow() { if (!running()) { visible_ = true; screen_ = Screen::Workflow; draw(); } }
void ScanUi::showSettings() { if (!running()) { visible_ = true; screen_ = Screen::SettingsMenu; draw(); } }

void ScanUi::cancelInteraction() {
    if (touchAction_ == TouchAction::Jog) stopJog();
    if (touchAction_ == TouchAction::Slider) saveProfile();
    touchAction_ = TouchAction::None;
    touchWasDown_ = false;
    visible_ = false;
}

void ScanUi::drawHeader(const char* title, bool back) {
    display_.fillRect(left, 0, 480 - left, 48, surface);
    display_.fillRect(left, 46, 480 - left, 2, lineColor);
    if (back) drawButton(62, 8, 58, 32, "<", false);
    display_.setTextDatum(back ? ML_DATUM : MC_DATUM);
    display_.setTextColor(text, surface);
    drawUiText(display_, title, back ? 134 : 266, 24, true);
}

void ScanUi::drawButton(int16_t x, int16_t y, int16_t w, int16_t h, const String& label, bool active, bool enabled) {
    const uint16_t fill = enabled ? raised : surface;
    display_.fillRoundRect(x, y, w, h, 6, fill);
    display_.drawRoundRect(x, y, w, h, 6, active ? cyan : lineColor);
    display_.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 5, active ? cyan : lineColor);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(enabled ? (active ? text : cyan) : muted, fill);
    display_.setFreeFont(&FreeSansBold12pt7b);
    display_.drawString(label, x + w / 2, y + h / 2);
    display_.setTextFont(1);
}

void ScanUi::drawWorkflow(const ScanMachineStatus* machine) {
    drawHeader("SCAN");
    const ScanMachineStatus& status = machine == nullptr ? machine_ : *machine;
    const bool canCapture = controlsAvailable(status);
    drawButton(76, 52, 380, 42, sessionStartSet_ ? "XY START NEU SETZEN" : "XY START SETZEN", sessionStartSet_, canCapture);
    drawButton(76, 100, 380, 42, sessionEndSet_ ? "XY ENDE NEU SETZEN" : "XY ENDE SETZEN", sessionEndSet_, canCapture);
    drawButton(76, 148, 380, 42, sessionFocusStartSet_ ? "Z START NEU SETZEN" : "Z START SETZEN", sessionFocusStartSet_, canCapture);
    drawButton(76, 196, 380, 42, sessionFocusEndSet_ ? "Z ENDE NEU SETZEN" : "Z ENDE SETZEN", sessionFocusEndSet_, canCapture);
    const bool ready = readyToScan(status);
    const String scanLabel = totalImages_ > 0 ? "SCAN STARTEN  /  " + String(totalImages_) : "SCAN STARTEN";
    drawButton(76, 248, 380, 60, scanLabel, false, ready);
}

void ScanUi::drawProgress() {
    drawHeader("SCAN LAEUFT");
    const int completed = min(currentIndex_, totalImages_);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(phase_ == Phase::Error ? red : cyan, background);
    drawUiText(display_, phaseLabel(static_cast<int>(phase_)), 266, 76, true);
    display_.setTextColor(text, background);
    drawUiText(display_, String(completed) + " / " + totalImages_, 266, 118, true);
    display_.fillRoundRect(78, 164, 376, 14, 7, lineColor);
    display_.drawRoundRect(77, 163, 378, 16, 8, text);
    const int16_t width = totalImages_ ? 376 * completed / totalImages_ : 0;
    if (width) display_.fillRoundRect(78, 164, width, 14, 7, phase_ == Phase::Done ? green : cyan);
    if (phase_ == Phase::Done || phase_ == Phase::Error) {
        drawButton(156, 242, 220, 50, "SCHLIESSEN", phase_ == Phase::Done);
    } else {
        drawButton(76, 236, 178, 56, phase_ == Phase::Paused || pauseRequested_ ? "FORTSETZEN" : "PAUSE", phase_ == Phase::Paused || pauseRequested_);
        drawButton(278, 236, 178, 56, "STOP", true);
    }
}

void ScanUi::drawSettingsMenu() {
    drawHeader("EINSTELLUNGEN");
    drawButton(68, 54, 190, 46, "BILDFELD", profile_.frameXSet && profile_.frameYSet);
    drawButton(274, 54, 190, 46, "OVERLAP " + String(profile_.overlap) + "%");
    drawButton(68, 108, 190, 46, "TEMPO " + String(profile_.speed));
    drawButton(274, 108, 190, 46, "RUHE " + String(profile_.settleMs));
    drawButton(68, 162, 190, 46, "SCHAERFE " + String(profile_.focusSteps));
    drawButton(274, 162, 190, 46, "KAMERA", profile_.cameraEnabled);
    drawButton(68, 216, 190, 46, "AUFLOESUNG");
    drawButton(274, 216, 190, 46, profile_.returnToStart ? "RUECKKEHR AN" : "RUECKKEHR AUS", profile_.returnToStart);
}

void ScanUi::drawFieldMenu() {
    drawHeader("BILDFELD", true);
    drawButton(86, 82, 360, 76, profile_.frameXSet ? "X NEU VERMESSEN" : "X VERMESSEN", profile_.frameXSet);
    drawButton(86, 184, 360, 76, profile_.frameYSet ? "Y NEU VERMESSEN" : "Y VERMESSEN", profile_.frameYSet);
}

void ScanUi::drawAxisArrow(int16_t centerX, bool positive, bool pressed, bool enabled) {
    const uint16_t fill = enabled ? raised : surface;
    const uint16_t arrowColor = enabled ? cyan : muted;
    display_.fillRoundRect(centerX - 48, 112, 96, 96, 8, fill);
    display_.drawRoundRect(centerX - 48, 112, 96, 96, 8, pressed ? cyan : lineColor);
    display_.drawRoundRect(centerX - 47, 113, 94, 94, 7, pressed ? cyan : lineColor);
    if (positive) display_.fillTriangle(centerX + 24, 160, centerX - 14, 136, centerX - 14, 184, arrowColor);
    else display_.fillTriangle(centerX - 24, 160, centerX + 14, 136, centerX + 14, 184, arrowColor);
}

void ScanUi::drawCalibration(char axis) {
    const String title = String("BILDFELD ") + axis;
    drawHeader(title.c_str(), true);
    const bool available = machine_.connected && machine_.positionValid;
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(muted, background);
    const String instruction = calibrationASet_ ? "ZUR GEGENUEBERLIEGENDEN KANTE FAHREN" : "ERSTE BILDFELDKANTE ANFAHREN";
    drawUiText(display_, instruction, 266, 66);
    drawAxisArrow(136, false, false, available); drawAxisArrow(396, true, false, available);
    display_.setTextColor(text, background);
    drawUiText(display_, String(axis) + "-ACHSE", 266, 160, true);
    drawButton(116, 232, 300, 56, calibrationASet_ ? "ENDE UEBERNEHMEN" : "START UEBERNEHMEN", false, available);
    const bool valid = axis == 'X' ? profile_.frameXSet : profile_.frameYSet;
    if (valid) {
        display_.setTextColor(green, background);
        const float value = axis == 'X' ? profile_.frameX : profile_.frameY;
        drawUiText(display_, String(value, 4) + " mm", 266, 300);
    }
}

void ScanUi::drawParameter() {
    const char* title = parameter_ == Parameter::Overlap ? "OVERLAP" : parameter_ == Parameter::Speed ? "TEMPO" :
        (parameter_ == Parameter::Settle ? "RUHEZEIT" : "SCHAERFESCHRITTE");
    drawHeader(title, true);
    drawParameterControl();
    drawButton(70, 210, 120, 70, "-", false); drawButton(342, 210, 120, 70, "+", false);
}

void ScanUi::drawParameterControl() {
    display_.fillRect(80, 62, 372, 58, background);
    display_.fillRect(120, 140, 300, 45, background);
    int value, minimum, maximum;
    String unit;
    if (parameter_ == Parameter::Overlap) { value = profile_.overlap; minimum = 0; maximum = 80; unit = "%"; }
    else if (parameter_ == Parameter::Speed) { value = profile_.speed; minimum = 1; maximum = scanMaxSpeed; unit = " mm/min"; }
    else if (parameter_ == Parameter::Settle) { value = profile_.settleMs; minimum = 0; maximum = 2000; unit = " ms"; }
    else { value = profile_.focusSteps; minimum = 1; maximum = maxFocusSteps; unit = " SCHRITTE"; }
    display_.setTextDatum(MC_DATUM); display_.setTextColor(text, background);
    drawUiText(display_, String(value) + unit, 266, 92, true);
    display_.fillRoundRect(sliderLeft, 155, sliderRight - sliderLeft, 14, 7, lineColor);
    const int16_t handle = map(value, minimum, maximum, sliderLeft, sliderRight);
    if (handle > sliderLeft) display_.fillRoundRect(sliderLeft, 155, handle - sliderLeft, 14, 7, cyan);
    display_.fillCircle(handle, 162, 16, cyan); display_.fillCircle(handle, 162, 7, text);
}

void ScanUi::drawCamera() {
    drawHeader("KAMERA", true);
    display_.setTextDatum(MC_DATUM); display_.setTextColor(muted, background);
    drawUiText(display_, "IO38  /  FESTER IMPULS 50 ms", 266, 74);
    display_.setTextColor(profile_.cameraEnabled ? green : muted, background);
    drawUiText(display_, profile_.cameraEnabled ? "IO38 AKTIV" : "IO38 INAKTIV", 266, 112, true);
    drawButton(86, 150, 360, 58, profile_.cameraEnabled ? "KAMERA: AN" : "KAMERA: AUS", profile_.cameraEnabled);
    drawButton(86, 228, 360, 58, cameraTestActive_ ? "IMPULS AKTIV" : "TESTIMPULS", cameraTestActive_, profile_.cameraEnabled);
}

void ScanUi::drawResolution() {
    drawHeader("AUFLOESUNG", true);
    const int widths[] = {1920, 3840, 5472}; const int heights[] = {1080, 2160, 3648};
    for (int i = 0; i < 3; ++i) drawButton(86, 70 + i * 72, 360, 56, String(widths[i]) + " x " + heights[i] + " px", profile_.cameraWidth == widths[i]);
}

void ScanUi::draw() {
    if (!visible_) return;
    display_.fillRect(left, 0, 480 - left, 320, background);
    if (screen_ == Screen::Workflow) phase_ == Phase::Idle ? drawWorkflow() : drawProgress();
    else if (screen_ == Screen::SettingsMenu) drawSettingsMenu();
    else if (screen_ == Screen::FieldMenu) drawFieldMenu();
    else if (screen_ == Screen::CalibrateX) drawCalibration('X');
    else if (screen_ == Screen::CalibrateY) drawCalibration('Y');
    else if (screen_ == Screen::Parameter) drawParameter();
    else if (screen_ == Screen::Camera) drawCamera();
    else drawResolution();
}

void ScanUi::redraw() {
    if (!visible_) return;
    display_.fillRect(left, 0, 480 - left, 320, background);
    draw();
}

void ScanUi::openParameter(Parameter value) { parameter_ = value; screen_ = Screen::Parameter; redraw(); }

void ScanUi::startJog(JogDirection direction, char axis) {
    if (!machine_.connected) return;
    jogDirection_ = direction;
    jogAxis_ = axis;
    touchAction_ = TouchAction::Jog;
    sendJogSegment();
    drawAxisArrow(direction == JogDirection::Negative ? 136 : 396,
                  direction == JogDirection::Positive, true, true);
}

void ScanUi::sendJogSegment() {
    if (jogDirection_ == JogDirection::None) return;
    String command = "$J=G91 G21 ";
    command += jogAxis_;
    command += jogDirection_ == JogDirection::Positive ? "-0.20" : "0.20";
    command += " F60\r";
    controller_.write(reinterpret_cast<const uint8_t*>(command.c_str()), command.length());
    controller_.flush();
    lastJogAt_ = millis();
}

void ScanUi::stopJog() {
    controller_.write(jogCancel);
    jogDirection_ = JogDirection::None;
}

void ScanUi::captureCalibration(const ScanMachineStatus& machine, char axis) {
    if (!machine.connected || !machine.positionValid) return;
    const float position = axis == 'X' ? machine.x : (axis == 'Y' ? machine.y : machine.z);
    if (!calibrationASet_) { calibrationA_ = position; calibrationASet_ = true; }
    else {
        const float span = fabsf(position - calibrationA_);
        if (span <= 0.00001F) {
            return;
        }
        if (axis == 'X') { profile_.frameX = span; profile_.frameXSet = true; }
        else { profile_.frameY = span; profile_.frameYSet = true; }
        calibrationASet_ = false; saveProfile(); calculateGrid();
        screen_ = Screen::FieldMenu;
    }
    redraw();
}

void ScanUi::updateParameterFromX(int16_t x) {
    x = constrain(x, sliderLeft, sliderRight);
    const int previous = parameter_ == Parameter::Overlap ? profile_.overlap :
        (parameter_ == Parameter::Speed ? profile_.speed : (parameter_ == Parameter::Settle ? profile_.settleMs : profile_.focusSteps));
    if (parameter_ == Parameter::Overlap) profile_.overlap = map(x, sliderLeft, sliderRight, 0, 80);
    else if (parameter_ == Parameter::Speed) profile_.speed = map(x, sliderLeft, sliderRight, 1, scanMaxSpeed);
    else if (parameter_ == Parameter::Settle) profile_.settleMs = map(x, sliderLeft, sliderRight, 0, 40) * 50;
    else profile_.focusSteps = map(x, sliderLeft, sliderRight, 1, maxFocusSteps);
    const int current = parameter_ == Parameter::Overlap ? profile_.overlap :
        (parameter_ == Parameter::Speed ? profile_.speed : (parameter_ == Parameter::Settle ? profile_.settleMs : profile_.focusSteps));
    if (current == previous) return;
    calculateGrid();
    if (visible_ && screen_ == Screen::Parameter) drawParameterControl();
}

void ScanUi::changeParameter(int delta) {
    if (parameter_ == Parameter::Overlap) profile_.overlap = constrain(profile_.overlap + delta, 0, 80);
    else if (parameter_ == Parameter::Speed) profile_.speed = constrain(profile_.speed + delta, 1, scanMaxSpeed);
    else if (parameter_ == Parameter::Settle) profile_.settleMs = constrain(profile_.settleMs + delta * 50, 0, 2000);
    else profile_.focusSteps = constrain(profile_.focusSteps + delta, 1, maxFocusSteps);
    calculateGrid(); saveProfile(); redraw();
}

void ScanUi::calculateGrid() {
    if (!(profile_.frameXSet && profile_.frameYSet && sessionStartSet_ && sessionEndSet_)) { columns_ = rows_ = totalImages_ = 0; return; }
    const float strideX = profile_.frameX * (1.0F - profile_.overlap / 100.0F);
    const float strideY = profile_.frameY * (1.0F - profile_.overlap / 100.0F);
    columns_ = fabsf(sessionEndX_ - sessionStartX_) < 0.00001F ? 1 : static_cast<int>(ceilf(fabsf(sessionEndX_ - sessionStartX_) / strideX)) + 1;
    rows_ = fabsf(sessionEndY_ - sessionStartY_) < 0.00001F ? 1 : static_cast<int>(ceilf(fabsf(sessionEndY_ - sessionStartY_) / strideY)) + 1;
    totalImages_ = columns_ * rows_ * profile_.focusSteps;
}

bool ScanUi::readyToScan(const ScanMachineStatus& machine) const {
    return controlsAvailable(machine) &&
        profile_.frameXSet && profile_.frameYSet &&
        (profile_.focusSteps == 1 || (sessionFocusStartSet_ && sessionFocusEndSet_)) &&
        sessionStartSet_ && sessionEndSet_ && totalImages_ > 0;
}

bool ScanUi::controlsAvailable(const ScanMachineStatus& machine) const {
    return machine.connected && machine.motion == ScanMotionState::Idle && machine.positionValid;
}

void ScanUi::sendLine(const String& value) {
    String line = value + '\r'; controller_.write(reinterpret_cast<const uint8_t*>(line.c_str()), line.length()); controller_.flush();
}

void ScanUi::sendMove(float x, float y, float z) {
    sendLine("G90 G21 G1 X" + String(x, 4) + " Y" + String(y, 4) + " Z" + String(z, 4) + " F" + String(profile_.speed));
}

void ScanUi::targetForIndex(int index, float& x, float& y, float& z) const {
    const int xyIndex = index / profile_.focusSteps;
    const int focusIndex = index % profile_.focusSteps;
    const int row = xyIndex / columns_; int column = xyIndex % columns_;
    if (row & 1) column = columns_ - 1 - column;
    const float spanX = fabsf(sessionEndX_ - sessionStartX_);
    const float spanY = fabsf(sessionEndY_ - sessionStartY_);
    const float strideX = profile_.frameX * (1.0F - profile_.overlap / 100.0F);
    const float strideY = profile_.frameY * (1.0F - profile_.overlap / 100.0F);
    const float offsetX = min(spanX, column * strideX);
    const float offsetY = min(spanY, row * strideY);
    x = sessionStartX_ + (sessionEndX_ >= sessionStartX_ ? offsetX : -offsetX);
    y = sessionStartY_ + (sessionEndY_ >= sessionStartY_ ? offsetY : -offsetY);
    z = returnZ_;
    if (profile_.focusSteps > 1) {
        z = sessionFocusStartZ_ +
            (sessionFocusEndZ_ - sessionFocusStartZ_) * focusIndex / (profile_.focusSteps - 1);
    }
}

void ScanUi::startScan(const ScanMachineStatus& machine) {
    calculateGrid();
    if (!readyToScan(machine)) return;
    returnX_ = machine.x; returnY_ = machine.y; returnZ_ = machine.z; currentIndex_ = 0; pauseRequested_ = false;
    releaseTrigger(); phase_ = Phase::SendMove; phaseStartedAt_ = millis(); redraw();
}

void ScanUi::releaseTrigger() { sendLine("M65 P0"); triggerOutputActive_ = false; cameraTestActive_ = false; }

void ScanUi::stopScan() {
    releaseTrigger();
    controller_.write('!'); controller_.write(softReset);
    phase_ = Phase::Error; pauseRequested_ = false; redraw();
}

void ScanUi::togglePause() {
    if (phase_ == Phase::Paused) { pauseRequested_ = false; phase_ = Phase::SendMove; phaseStartedAt_ = millis(); }
    else pauseRequested_ = !pauseRequested_;
    redraw();
}

void ScanUi::startCameraTest(const ScanMachineStatus& machine) {
    if (!profile_.cameraEnabled || !machine.connected) return;
    if (cameraTestActive_) return;
    sendLine("M64 P0"); triggerOutputActive_ = true; cameraTestActive_ = true; phaseStartedAt_ = millis(); redraw();
}

void ScanUi::advanceScan() {
    ++currentIndex_;
    if (currentIndex_ >= totalImages_) {
        if (profile_.returnToStart) { sendMove(returnX_, returnY_, returnZ_); phase_ = Phase::ReturnStart; moveObserved_ = false; phaseStartedAt_ = millis(); }
        else phase_ = Phase::Done;
    } else phase_ = pauseRequested_ ? Phase::Paused : Phase::SendMove;
    redraw();
}

void ScanUi::service(const ScanMachineStatus& machine) {
    const uint32_t now = millis();
    machine_ = machine;
    if (touchAction_ == TouchAction::Jog && now - lastJogAt_ >= 150) sendJogSegment();
    if (cameraTestActive_ && now - phaseStartedAt_ >= cameraPulseMs) { releaseTrigger(); if (screen_ == Screen::Camera) redraw(); }
    if (phase_ == Phase::Idle || phase_ == Phase::Done || phase_ == Phase::Error) return;
    if (!machine.connected || machine.motion == ScanMotionState::Blocked) { stopScan(); return; }
    if (phase_ == Phase::Paused) return;
    if (phase_ == Phase::SendMove) {
        float x, y, z; targetForIndex(currentIndex_, x, y, z); sendMove(x, y, z); moveObserved_ = false; phase_ = Phase::WaitMove; phaseStartedAt_ = now; redraw();
    } else if (phase_ == Phase::WaitMove) {
        if (machine.motion == ScanMotionState::Moving) moveObserved_ = true;
        if (machine.motion == ScanMotionState::Idle && (moveObserved_ || now - phaseStartedAt_ > 350)) { phase_ = Phase::Settle; phaseStartedAt_ = now; redraw(); }
    } else if (phase_ == Phase::Settle && now - phaseStartedAt_ >= static_cast<uint32_t>(profile_.settleMs)) {
        if (profile_.cameraEnabled) { sendLine("M64 P0"); triggerOutputActive_ = true; phase_ = Phase::TriggerOn; phaseStartedAt_ = now; redraw(); }
        else advanceScan();
    } else if (phase_ == Phase::TriggerOn && now - phaseStartedAt_ >= cameraPulseMs) {
        releaseTrigger(); phase_ = Phase::TriggerOff; phaseStartedAt_ = now; redraw();
    } else if (phase_ == Phase::TriggerOff && now - phaseStartedAt_ >= 30) advanceScan();
    else if (phase_ == Phase::ReturnStart) {
        if (machine.motion == ScanMotionState::Moving) moveObserved_ = true;
        if (machine.motion == ScanMotionState::Idle && (moveObserved_ || now - phaseStartedAt_ > 350)) { phase_ = Phase::Done; redraw(); }
    }
}

void ScanUi::onPress(int16_t x, int16_t y, const ScanMachineStatus& machine) {
    if (screen_ == Screen::Workflow && phase_ != Phase::Idle) {
        if ((phase_ == Phase::Done || phase_ == Phase::Error) && inside(x, y, 156, 242, 220, 50)) { phase_ = Phase::Idle; redraw(); }
        else if (phase_ != Phase::Done && phase_ != Phase::Error) {
            if (inside(x, y, 76, 236, 178, 56)) togglePause(); else if (inside(x, y, 278, 236, 178, 56)) stopScan();
        }
        return;
    }
    if (screen_ != Screen::Workflow && screen_ != Screen::SettingsMenu && inside(x, y, 62, 8, 58, 32)) {
        calibrationASet_ = false;
        screen_ = (screen_ == Screen::CalibrateX || screen_ == Screen::CalibrateY)
            ? Screen::FieldMenu : Screen::SettingsMenu;
        redraw(); return;
    }
    if (screen_ == Screen::Workflow) {
        if (inside(x, y, 76, 52, 380, 42)) {
            if (controlsAvailable(machine)) {
                sessionStartX_ = machine.x; sessionStartY_ = machine.y; sessionStartSet_ = true; calculateGrid(); redraw();
            }
        } else if (inside(x, y, 76, 100, 380, 42)) {
            if (controlsAvailable(machine)) {
                sessionEndX_ = machine.x; sessionEndY_ = machine.y; sessionEndSet_ = true; calculateGrid(); redraw();
            }
        } else if (inside(x, y, 76, 148, 380, 42)) {
            if (controlsAvailable(machine)) {
                sessionFocusStartZ_ = machine.z; sessionFocusStartSet_ = true; redraw();
            }
        } else if (inside(x, y, 76, 196, 380, 42)) {
            if (controlsAvailable(machine)) {
                sessionFocusEndZ_ = machine.z; sessionFocusEndSet_ = true; redraw();
            }
        } else if (inside(x, y, 76, 248, 380, 60)) startScan(machine);
    } else if (screen_ == Screen::SettingsMenu) {
        if (inside(x, y, 68, 54, 190, 46)) { screen_ = Screen::FieldMenu; redraw(); }
        else if (inside(x, y, 274, 54, 190, 46)) openParameter(Parameter::Overlap);
        else if (inside(x, y, 68, 108, 190, 46)) openParameter(Parameter::Speed);
        else if (inside(x, y, 274, 108, 190, 46)) openParameter(Parameter::Settle);
        else if (inside(x, y, 68, 162, 190, 46)) openParameter(Parameter::FocusSteps);
        else if (inside(x, y, 274, 162, 190, 46)) { screen_ = Screen::Camera; redraw(); }
        else if (inside(x, y, 68, 216, 190, 46)) { screen_ = Screen::Resolution; redraw(); }
        else if (inside(x, y, 274, 216, 190, 46)) { profile_.returnToStart = !profile_.returnToStart; saveProfile(); redraw(); }
    } else if (screen_ == Screen::FieldMenu) {
        calibrationASet_ = false;
        if (inside(x, y, 86, 82, 360, 76)) { screen_ = Screen::CalibrateX; redraw(); }
        else if (inside(x, y, 86, 184, 360, 76)) { screen_ = Screen::CalibrateY; redraw(); }
    } else if (screen_ == Screen::CalibrateX || screen_ == Screen::CalibrateY) {
        const char axis = screen_ == Screen::CalibrateX ? 'X' : 'Y';
        if (inside(x, y, 88, 112, 96, 96)) startJog(JogDirection::Negative, axis);
        else if (inside(x, y, 348, 112, 96, 96)) startJog(JogDirection::Positive, axis);
        else if (inside(x, y, 116, 232, 300, 56)) captureCalibration(machine, axis);
    } else if (screen_ == Screen::Parameter) {
        if (inside(x, y, 130, 135, 271, 55)) { touchAction_ = TouchAction::Slider; updateParameterFromX(x); }
        else if (inside(x, y, 70, 210, 120, 70)) changeParameter(-1);
        else if (inside(x, y, 342, 210, 120, 70)) changeParameter(1);
    } else if (screen_ == Screen::Camera) {
        if (inside(x, y, 86, 150, 360, 58)) { profile_.cameraEnabled = !profile_.cameraEnabled; if (!profile_.cameraEnabled) releaseTrigger(); saveProfile(); redraw(); }
        else if (inside(x, y, 86, 228, 360, 58)) startCameraTest(machine);
    } else if (screen_ == Screen::Resolution) {
        if (inside(x, y, 86, 70, 360, 56)) { profile_.cameraWidth = 1920; profile_.cameraHeight = 1080; }
        else if (inside(x, y, 86, 142, 360, 56)) { profile_.cameraWidth = 3840; profile_.cameraHeight = 2160; }
        else if (inside(x, y, 86, 214, 360, 56)) { profile_.cameraWidth = 5472; profile_.cameraHeight = 3648; }
        else return;
        saveProfile(); redraw();
    }
}

void ScanUi::onDrag(int16_t x) { if (touchAction_ == TouchAction::Slider) updateParameterFromX(x); }

void ScanUi::onRelease() {
    if (touchAction_ == TouchAction::Jog) { stopJog(); redraw(); }
    if (touchAction_ == TouchAction::Slider) saveProfile();
    touchAction_ = TouchAction::None;
}

void ScanUi::handleTouch(bool touched, int16_t x, int16_t y, const ScanMachineStatus& machine) {
    if (touched && !touchWasDown_) onPress(x, y, machine);
    else if (touched && touchWasDown_) onDrag(x);
    else if (!touched && touchWasDown_) onRelease();
    touchWasDown_ = touched;
}

bool ScanUi::running() const { return phase_ != Phase::Idle && phase_ != Phase::Done && phase_ != Phase::Error; }
