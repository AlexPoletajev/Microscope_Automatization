#include "slip_test_ui.h"

#include <Preferences.h>

namespace {
constexpr int16_t left = 52;
constexpr int16_t sliderLeft = 145;
constexpr int16_t sliderRight = 386;
constexpr int minSpeed = 1;
constexpr int maxSpeed = 600;
constexpr int minRounds = 1;
constexpr int maxRounds = 100;
constexpr uint8_t softReset = 0x18;

uint16_t background, surface, raised, lineColor, muted, text, cyan, green, red;

bool inside(int16_t x, int16_t y, int16_t bx, int16_t by, int16_t width, int16_t height) {
    return x >= bx && x <= bx + width && y >= by && y <= by + height;
}

void drawUiText(TFT_eSPI& display, const String& value, int16_t x, int16_t y, bool large = false) {
    display.setFreeFont(large ? &FreeSans12pt7b : &FreeSans9pt7b);
    display.drawString(value, x, y);
    display.setTextFont(1);
}
}  // namespace

SlipTestUi::SlipTestUi(TFT_eSPI& display, HardwareSerial& controller, ScanUi& scanUi)
    : display_(display), controller_(controller), scanUi_(scanUi) {}

void SlipTestUi::begin() {
    background = display_.color565(4, 6, 8);
    surface = display_.color565(22, 27, 31);
    raised = display_.color565(55, 65, 72);
    lineColor = display_.color565(105, 119, 128);
    muted = display_.color565(205, 215, 220);
    text = display_.color565(255, 255, 255);
    cyan = display_.color565(96, 226, 255);
    green = display_.color565(79, 229, 145);
    red = display_.color565(255, 91, 91);
    loadSettings();
}

void SlipTestUi::loadSettings() {
    Preferences p;
    if (!p.begin("slip", true)) return;
    speed_ = constrain(p.getInt("speed", 100), minSpeed, maxSpeed);
    rounds_ = constrain(p.getInt("rounds", 10), minRounds, maxRounds);
    p.end();
}

void SlipTestUi::saveSettings() {
    Preferences p;
    if (!p.begin("slip", false)) return;
    p.putInt("speed", speed_);
    p.putInt("rounds", rounds_);
    p.end();
}

void SlipTestUi::show() {
    if (running()) return;
    visible_ = true;
    screen_ = Screen::Menu;
    draw();
}

void SlipTestUi::cancelInteraction() {
    if (touchAction_ == TouchAction::Slider) saveSettings();
    touchAction_ = TouchAction::None;
    touchWasDown_ = false;
    visible_ = false;
}

void SlipTestUi::drawHeader(const char* title, bool back) {
    display_.fillRect(left, 0, 480 - left, 48, surface);
    display_.fillRect(left, 46, 480 - left, 2, lineColor);
    if (back) drawButton(62, 8, 58, 32, "<");
    display_.setTextDatum(back ? ML_DATUM : MC_DATUM);
    display_.setTextColor(text, surface);
    drawUiText(display_, title, back ? 134 : 266, 24, true);
}

void SlipTestUi::drawButton(int16_t x, int16_t y, int16_t width, int16_t height,
                            const String& label, bool active, bool enabled) {
    const uint16_t fill = enabled ? raised : surface;
    display_.fillRoundRect(x, y, width, height, 6, fill);
    display_.drawRoundRect(x, y, width, height, 6, active ? cyan : lineColor);
    display_.drawRoundRect(x + 1, y + 1, width - 2, height - 2, 5, active ? cyan : lineColor);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(enabled ? (active ? text : cyan) : muted, fill);
    display_.setFreeFont(&FreeSansBold12pt7b);
    display_.drawString(label, x + width / 2, y + height / 2);
    display_.setTextFont(1);
}

void SlipTestUi::drawWorkflow() {
    drawHeader("SCHLUPFTEST", true);
    const bool available = controlsAvailable(machine_);
    drawButton(66, 54, 190, 72, "PUNKT A + Z", pointASet_, available);
    drawButton(276, 54, 190, 72, "PUNKT B + Z", pointBSet_, available);
    drawButton(66, 136, 190, 72, "TEMPO  " + String(speed_));
    drawButton(276, 136, 190, 72, "RUNDEN  " + String(rounds_));
    drawButton(116, 228, 300, 72, "TEST STARTEN", false, available && pointASet_ && pointBSet_);
}

void SlipTestUi::drawMenu() {
    drawHeader("TESTS");
    drawButton(66, 54, 190, 72, "UEBERSICHT");
    drawButton(276, 54, 190, 72, "SCAN-SCHRITTE");
    drawButton(66, 136, 190, 72, "BILDFELD");
    drawButton(276, 136, 190, 72, "SCANBEREICH");
    drawButton(116, 218, 300, 80, "SCHLUPFTEST");
}

void SlipTestUi::drawOverview() {
    drawHeader("SCAN-UEBERSICHT", true);
    const ScanOverview value = scanUi_.overview();
    String edge;
    if (!value.uniformX && value.columns > 0) edge += "X";
    if (!value.uniformY && value.rows > 0) edge += "Y";
    const String actualOverlap = value.columns > 0 && value.rows > 0
        ? "OVERLAP IST  X " + String(value.actualOverlapX, 1) + "%  Y " + String(value.actualOverlapY, 1) + "%" +
            (edge.length() ? "  RAND:" + edge : "")
        : "OVERLAP IST  --";
    const String lines[] = {
        "BILDFELD  X " + String(value.frameX, 4) + "  Y " + String(value.frameY, 4) + " mm",
        "BEREICH   X " + String(value.rangeX, 3) + "  Y " + String(value.rangeY, 3) + " mm",
        "Z-BEREICH  " + String(value.rangeZ, 4) + " mm",
        "OVERLAP SOLL  " + String(value.overlapMin) + "-" + String(value.overlapMax) + "%",
        actualOverlap,
        "SCHRITT  X " + String(value.stepX, 3) + "  Y " + String(value.stepY, 3) + "  Z " + String(value.stepZ, 3),
        "RASTER  X " + String(value.columns) + "  Y " + String(value.rows) + "  Z " + String(value.focusSteps) +
            "  = " + String(value.totalImages),
        "TEMPO XY " + String(value.speed) + "  Z " + String(value.zSpeed) + " mm/min",
        "RUHE " + String(value.settleMs) + " ms   KAMERA " + String(value.cameraEnabled ? "AN" : "AUS"),
        "IMPULS " + String(value.cameraPulseMs) + " ms",
        value.timingMarkers ? "MARKER  Z NORMAL  X 8 s  Y 20 s" : "MARKER  AUS",
        "XY-FOLGE " + String(value.columnMajor ? "SPALTEN" : "ZEILEN") +
            "   Z " + String(value.focusSerpentine ? "PENDEL" : "START-ENDE"),
        "AUFLOESUNG " + String(value.cameraWidth) + "x" + String(value.cameraHeight) +
            "   RUECK " + String(value.returnToStart ? "AN" : "AUS")
    };
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(text, background);
    display_.setFreeFont(&FreeSansBold9pt7b);
    for (int index = 0; index < 13; ++index) display_.drawString(lines[index], 266, 48 + index * 20);
    display_.setTextFont(1);
}

float SlipTestUi::testDistance(char axis) const {
    if (screen_ == Screen::StepTest) return scanUi_.scanStep(axis);
    if (screen_ == Screen::FrameTest) return scanUi_.frameDistance(axis);
    if (screen_ == Screen::RangeTest) return scanUi_.scanRange(axis);
    return 0.0F;
}

void SlipTestUi::drawDistanceTest() {
    const char* title = screen_ == Screen::StepTest ? "SCAN-SCHRITTE" :
        "BILDFELDWEITEN";
    drawHeader(title, true);
    const bool available = controlsAvailable(machine_);
    const char axes[] = {'X', 'Y', 'Z'};
    const int16_t rows[] = {58, 142, 226};
    for (int index = 0; index < 3; ++index) {
        const float distance = testDistance(axes[index]);
        drawButton(76, rows[index], 82, 58, "-", false, available && distance > 0.00001F);
        drawButton(374, rows[index], 82, 58, "+", false, available && distance > 0.00001F);
        display_.setTextDatum(MC_DATUM);
        display_.setTextColor(distance > 0.00001F ? text : muted, background);
        display_.setFreeFont(&FreeSansBold12pt7b);
        const String value = distance > 0.00001F ? String(distance, 4) + " mm" : "NICHT GESETZT";
        display_.drawString(String(axes[index]) + "  " + value, 266, rows[index] + 29);
        display_.setTextFont(1);
    }
}

void SlipTestUi::drawRangeTest() {
    drawHeader("SCANBEREICH", true);
    const bool available = controlsAvailable(machine_);
    const char axes[] = {'X', 'Y', 'Z'};
    const int16_t rows[] = {58, 142, 226};
    for (int index = 0; index < 3; ++index) {
        const bool enabled = available && scanUi_.rangeEndpointAvailable(axes[index]);
        drawButton(76, rows[index], 180, 58, String(axes[index]) + " START", false, enabled);
        drawButton(276, rows[index], 180, 58, String(axes[index]) + " ENDE", false, enabled);
    }
}

void SlipTestUi::drawProgress() {
    drawHeader(phase_ == Phase::Done ? "TEST BEENDET" :
        (phase_ == Phase::Error ? "TEST ABGEBROCHEN" : (phase_ == Phase::Recover ? "TEST STOPPT" : "SCHLUPFTEST")));
    const int completedRounds = phase_ == Phase::Done ? rounds_ : min(rounds_, max(0, (targetIndex_ - 1) / 2));
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(phase_ == Phase::Error ? red : (phase_ == Phase::Done ? green : cyan), background);
    const String target = targetIndex_ == 0 || (targetIndex_ % 2 == 0) ? "PUNKT A" : "PUNKT B";
    drawUiText(display_, phase_ == Phase::Done ? "WIEDER AN PUNKT A" : target, 266, 82, true);
    display_.setTextColor(text, background);
    drawUiText(display_, String(completedRounds) + " / " + String(rounds_) + " RUNDEN", 266, 128, true);
    display_.fillRoundRect(78, 164, 376, 14, 7, lineColor);
    display_.drawRoundRect(77, 163, 378, 16, 8, text);
    const int totalTargets = rounds_ * 2 + 1;
    const int completedTargets = phase_ == Phase::Done ? totalTargets : min(targetIndex_, totalTargets);
    const int16_t width = 376 * completedTargets / totalTargets;
    if (width > 0) display_.fillRoundRect(78, 164, width, 14, 7, phase_ == Phase::Done ? green : cyan);
    if (phase_ == Phase::Done || phase_ == Phase::Error) {
        drawButton(156, 236, 220, 56, "SCHLIESSEN", phase_ == Phase::Done);
    } else if (phase_ == Phase::Recover) {
        drawButton(156, 236, 220, 56, "STOPPE...", false, false);
    } else {
        drawButton(156, 236, 220, 56, "STOP", true);
    }
}

void SlipTestUi::drawParameter() {
    drawHeader(screen_ == Screen::Speed ? "TESTTEMPO" : "RUNDEN", true);
    drawParameterControl();
    drawButton(70, 210, 120, 70, "-");
    drawButton(342, 210, 120, 70, "+");
}

void SlipTestUi::drawParameterControl() {
    display_.fillRect(80, 62, 372, 58, background);
    display_.fillRect(120, 140, 300, 45, background);
    const int value = screen_ == Screen::Speed ? speed_ : rounds_;
    const int minimum = screen_ == Screen::Speed ? minSpeed : minRounds;
    const int maximum = screen_ == Screen::Speed ? maxSpeed : maxRounds;
    const String unit = screen_ == Screen::Speed ? " mm/min" : " RUNDEN";
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(text, background);
    drawUiText(display_, String(value) + unit, 266, 92, true);
    display_.fillRoundRect(sliderLeft, 155, sliderRight - sliderLeft, 14, 7, lineColor);
    const int16_t handle = map(value, minimum, maximum, sliderLeft, sliderRight);
    if (handle > sliderLeft) display_.fillRoundRect(sliderLeft, 155, handle - sliderLeft, 14, 7, cyan);
    display_.fillCircle(handle, 162, 16, cyan);
    display_.fillCircle(handle, 162, 7, text);
}

void SlipTestUi::draw() {
    if (!visible_) return;
    display_.fillRect(left, 0, 480 - left, 320, background);
    if (phase_ != Phase::Idle) drawProgress();
    else if (screen_ == Screen::Menu) drawMenu();
    else if (screen_ == Screen::Overview) drawOverview();
    else if (screen_ == Screen::Workflow) drawWorkflow();
    else if (screen_ == Screen::RangeTest) drawRangeTest();
    else if (screen_ == Screen::StepTest || screen_ == Screen::FrameTest) drawDistanceTest();
    else drawParameter();
}

void SlipTestUi::redraw() {
    if (!visible_) return;
    draw();
}

bool SlipTestUi::controlsAvailable(const ScanMachineStatus& machine) const {
    return machine.connected && machine.positionValid && machine.motion == ScanMotionState::Idle;
}

void SlipTestUi::capturePoint(bool pointA, const ScanMachineStatus& machine) {
    if (!controlsAvailable(machine)) return;
    if (pointA) {
        pointAX_ = machine.x; pointAY_ = machine.y; pointAZ_ = machine.z; pointASet_ = true;
    } else {
        pointBX_ = machine.x; pointBY_ = machine.y; pointBZ_ = machine.z; pointBSet_ = true;
    }
    redraw();
}

void SlipTestUi::updateParameterFromX(int16_t x) {
    x = constrain(x, sliderLeft, sliderRight);
    const int previous = screen_ == Screen::Speed ? speed_ : rounds_;
    if (screen_ == Screen::Speed) speed_ = map(x, sliderLeft, sliderRight, minSpeed, maxSpeed);
    else rounds_ = map(x, sliderLeft, sliderRight, minRounds, maxRounds);
    if ((screen_ == Screen::Speed ? speed_ : rounds_) != previous) drawParameterControl();
}

void SlipTestUi::changeParameter(int delta) {
    if (screen_ == Screen::Speed) speed_ = constrain(speed_ + delta, minSpeed, maxSpeed);
    else rounds_ = constrain(rounds_ + delta, minRounds, maxRounds);
    saveSettings();
    redraw();
}

void SlipTestUi::sendMove(bool pointA) {
    const float x = pointA ? pointAX_ : pointBX_;
    const float y = pointA ? pointAY_ : pointBY_;
    const float z = pointA ? pointAZ_ : pointBZ_;
    const String command = "G90 G21 G1 X" + String(x, 4) + " Y" + String(y, 4) +
        " Z" + String(z, 4) + " F" + String(speed_) + '\r';
    controller_.write(reinterpret_cast<const uint8_t*>(command.c_str()), command.length());
    controller_.flush();
}

void SlipTestUi::startTest(const ScanMachineStatus& machine) {
    if (!controlsAvailable(machine) || !pointASet_ || !pointBSet_) return;
    targetIndex_ = 0;
    moveObserved_ = false;
    phase_ = Phase::SendMove;
    phaseStartedAt_ = millis();
    redraw();
}

void SlipTestUi::stopTest() {
    controller_.write('!');
    controller_.write(softReset);
    recoveryUnlockSent_ = false;
    lastRecoveryUnlockAt_ = 0;
    phase_ = Phase::Recover;
    phaseStartedAt_ = millis();
    redraw();
}

void SlipTestUi::closeResult() {
    phase_ = Phase::Idle;
    screen_ = Screen::Workflow;
    redraw();
}

void SlipTestUi::service(const ScanMachineStatus& machine) {
    machine_ = machine;
    const uint32_t now = millis();
    if (phase_ == Phase::Recover) {
        if (machine.connected && recoveryUnlockSent_ && machine.motion == ScanMotionState::Idle) {
            phase_ = Phase::Error;
            redraw();
            return;
        }
        if (machine.connected && now - phaseStartedAt_ >= 300 &&
            (!recoveryUnlockSent_ || now - lastRecoveryUnlockAt_ >= 500)) {
            const String unlock = "$X\r";
            controller_.write(reinterpret_cast<const uint8_t*>(unlock.c_str()), unlock.length());
            controller_.flush();
            recoveryUnlockSent_ = true;
            lastRecoveryUnlockAt_ = now;
        }
        return;
    }
    if (phase_ == Phase::Idle || phase_ == Phase::Done || phase_ == Phase::Error) return;
    if (!machine.connected || machine.motion == ScanMotionState::Blocked) {
        if (machine.connected) stopTest();
        else { phase_ = Phase::Error; redraw(); }
        return;
    }
    if (phase_ == Phase::SendMove) {
        sendMove(targetIndex_ % 2 == 0);
        moveObserved_ = false;
        phase_ = Phase::WaitMove;
        phaseStartedAt_ = now;
        redraw();
    } else if (phase_ == Phase::WaitMove) {
        if (machine.motion == ScanMotionState::Moving) moveObserved_ = true;
        if (machine.motion == ScanMotionState::Idle && (moveObserved_ || now - phaseStartedAt_ > 350)) {
            if (targetIndex_ >= rounds_ * 2) phase_ = Phase::Done;
            else { ++targetIndex_; phase_ = Phase::SendMove; }
            redraw();
        }
    }
}

void SlipTestUi::onPress(int16_t x, int16_t y, const ScanMachineStatus& machine) {
    if (phase_ != Phase::Idle) {
        if ((phase_ == Phase::Done || phase_ == Phase::Error) && inside(x, y, 156, 236, 220, 56)) closeResult();
        else if (phase_ != Phase::Done && phase_ != Phase::Error && phase_ != Phase::Recover &&
                 inside(x, y, 156, 236, 220, 56)) stopTest();
        return;
    }
    if (screen_ != Screen::Menu && inside(x, y, 62, 8, 58, 32)) {
        saveSettings();
        screen_ = (screen_ == Screen::Speed || screen_ == Screen::Rounds) ? Screen::Workflow : Screen::Menu;
        redraw();
        return;
    }
    if (screen_ == Screen::Menu) {
        if (inside(x, y, 66, 54, 190, 72)) { screen_ = Screen::Overview; redraw(); }
        else if (inside(x, y, 276, 54, 190, 72)) { screen_ = Screen::StepTest; redraw(); }
        else if (inside(x, y, 66, 136, 190, 72)) { screen_ = Screen::FrameTest; redraw(); }
        else if (inside(x, y, 276, 136, 190, 72)) { screen_ = Screen::RangeTest; redraw(); }
        else if (inside(x, y, 116, 218, 300, 80)) { screen_ = Screen::Workflow; redraw(); }
    } else if (screen_ == Screen::Workflow) {
        if (inside(x, y, 66, 54, 190, 72)) capturePoint(true, machine);
        else if (inside(x, y, 276, 54, 190, 72)) capturePoint(false, machine);
        else if (inside(x, y, 66, 136, 190, 72)) { screen_ = Screen::Speed; redraw(); }
        else if (inside(x, y, 276, 136, 190, 72)) { screen_ = Screen::Rounds; redraw(); }
        else if (inside(x, y, 116, 228, 300, 72)) startTest(machine);
    } else if (screen_ == Screen::RangeTest) {
        const char axes[] = {'X', 'Y', 'Z'};
        const int16_t rows[] = {58, 142, 226};
        for (int index = 0; index < 3; ++index) {
            if (inside(x, y, 76, rows[index], 180, 58)) {
                scanUi_.moveRangeEndpoint(axes[index], false, machine); redraw(); return;
            }
            if (inside(x, y, 276, rows[index], 180, 58)) {
                scanUi_.moveRangeEndpoint(axes[index], true, machine); redraw(); return;
            }
        }
    } else if (screen_ == Screen::StepTest || screen_ == Screen::FrameTest) {
        const char axes[] = {'X', 'Y', 'Z'};
        const int16_t rows[] = {58, 142, 226};
        for (int index = 0; index < 3; ++index) {
            if (inside(x, y, 76, rows[index], 82, 58)) {
                scanUi_.moveTestDistance(axes[index], testDistance(axes[index]), -1, machine); redraw(); return;
            }
            if (inside(x, y, 374, rows[index], 82, 58)) {
                scanUi_.moveTestDistance(axes[index], testDistance(axes[index]), 1, machine); redraw(); return;
            }
        }
    } else if (inside(x, y, 130, 135, 271, 55)) {
        touchAction_ = TouchAction::Slider;
        updateParameterFromX(x);
    } else if (inside(x, y, 70, 210, 120, 70)) changeParameter(-1);
    else if (inside(x, y, 342, 210, 120, 70)) changeParameter(1);
}

void SlipTestUi::onDrag(int16_t x) {
    if (touchAction_ == TouchAction::Slider) updateParameterFromX(x);
}

void SlipTestUi::onRelease() {
    if (touchAction_ == TouchAction::Slider) saveSettings();
    touchAction_ = TouchAction::None;
}

void SlipTestUi::handleTouch(bool touched, int16_t x, int16_t y, const ScanMachineStatus& machine) {
    if (touched && !touchWasDown_) onPress(x, y, machine);
    else if (touched && touchWasDown_) onDrag(x);
    else if (!touched && touchWasDown_) onRelease();
    touchWasDown_ = touched;
}

bool SlipTestUi::running() const {
    return phase_ == Phase::SendMove || phase_ == Phase::WaitMove || phase_ == Phase::Recover;
}
