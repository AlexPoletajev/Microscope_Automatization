#include "scan_ui.h"

#include <Preferences.h>
#include <cmath>

namespace {
constexpr int16_t left = 52;
constexpr int16_t sliderLeft = 145;
constexpr int16_t sliderRight = 386;
constexpr int scanMaxSpeed = 1000;
constexpr int scanMaxZSpeed = 6000;
constexpr int maxFocusSteps = 20;
constexpr int cameraRecoveryMs = 100;
constexpr uint8_t historyRowsPerPage = 4;
constexpr uint8_t jogCancel = 0x85;
constexpr uint8_t softReset = 0x18;
constexpr uint32_t jogCancelRepeatMs = 50;
constexpr uint32_t jogCancelGuardMs = 250;

uint16_t background, surface, raised, lineColor, muted, text, cyan, cyanDark, green, amber, red;

void drawUiText(TFT_eSPI& display, const String& value, int16_t x, int16_t y, bool large = false) {
    display.setFreeFont(large ? &FreeSans12pt7b : &FreeSans9pt7b);
    display.drawString(value, x, y);
    display.setTextFont(1);
}

bool inside(int16_t x, int16_t y, int16_t bx, int16_t by, int16_t w, int16_t h) {
    return x >= bx && x <= bx + w && y >= by && y <= by + h;
}

String formatDuration(uint32_t durationMs) {
    const uint32_t totalSeconds = durationMs / 1000;
    const uint32_t hours = totalSeconds / 3600;
    const uint32_t minutes = (totalSeconds / 60) % 60;
    const uint32_t seconds = totalSeconds % 60;
    char value[16];
    if (hours > 0) snprintf(value, sizeof(value), "%lu:%02lu:%02lu", hours, minutes, seconds);
    else snprintf(value, sizeof(value), "%02lu:%02lu", minutes, seconds);
    return String(value);
}

struct AxisLayout {
    AxisLayout(int countValue, float strideValue, bool uniformValue)
        : count(countValue), stride(strideValue), uniform(uniformValue) {}
    int count;
    float stride;
    bool uniform;
};

AxisLayout calculateAxisLayout(float span, float frame, int overlapMin, int overlapMax) {
    if (span < 0.00001F) return {1, 0.0F, true};
    const float lowStride = frame * (1.0F - overlapMin / 100.0F);
    const float highStride = frame * (1.0F - overlapMax / 100.0F);
    const int minimumIntervals = max(1, static_cast<int>(ceilf(span / lowStride - 0.00001F)));
    const int maximumIntervals = static_cast<int>(floorf(span / highStride + 0.00001F));
    if (minimumIntervals <= maximumIntervals) {
        const float targetOverlap = (overlapMin + overlapMax) * 0.5F;
        const float targetStride = frame * (1.0F - targetOverlap / 100.0F);
        const int intervals = constrain(static_cast<int>(roundf(span / targetStride)), minimumIntervals, maximumIntervals);
        return {intervals + 1, span / intervals, true};
    }
    const float targetOverlap = (overlapMin + overlapMax) * 0.5F;
    const float targetStride = frame * (1.0F - targetOverlap / 100.0F);
    return {static_cast<int>(ceilf(span / targetStride)) + 1, targetStride, false};
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
        case 10: return "STOPPE...";
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
    history_.begin();
    calculateGrid();
}

void ScanUi::loadProfile() {
    Preferences p;
    if (!p.begin("scan", true)) return;
    profile_.frameX = p.getFloat("frameX", 0.0F);
    profile_.frameY = p.getFloat("frameY", 0.0F);
    profile_.cameraWidth = p.getInt("camW", 1920);
    profile_.cameraHeight = p.getInt("camH", 1080);
    const int legacyOverlap = constrain(p.getInt("overlap", 15), 0, 80);
    profile_.overlapMin = constrain(p.getInt("ovMin", max(0, legacyOverlap - 5)), 0, 80);
    profile_.overlapMax = constrain(p.getInt("ovMax", min(80, legacyOverlap + 5)), 0, 80);
    if (profile_.overlapMin > profile_.overlapMax) {
        const int swap = profile_.overlapMin; profile_.overlapMin = profile_.overlapMax; profile_.overlapMax = swap;
    }
    profile_.speed = constrain(p.getInt("speed", 60), 1, scanMaxSpeed);
    profile_.zSpeed = constrain(p.getInt("zSpeed", profile_.speed), 1, scanMaxZSpeed);
    profile_.settleMs = constrain((p.getInt("settle", 300) + 25) / 50 * 50, 0, 2000);
    profile_.focusSteps = constrain(p.getInt("focusN", 1), 1, maxFocusSteps);
    profile_.cameraPulseMs = 15;
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
    p.putInt("ovMin", profile_.overlapMin); p.putInt("ovMax", profile_.overlapMax);
    p.putInt("speed", profile_.speed); p.putInt("zSpeed", profile_.zSpeed);
    p.putInt("settle", profile_.settleMs); p.putInt("focusN", profile_.focusSteps); p.putBool("camera", profile_.cameraEnabled);
    p.putInt("camPulse", profile_.cameraPulseMs);
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
    drawButton(66, 54, 190, 72, "XY START", sessionStartSet_, canCapture);
    drawButton(276, 54, 190, 72, "XY ENDE", sessionEndSet_, canCapture);
    drawButton(66, 136, 190, 72, "Z START", sessionFocusStartSet_, canCapture);
    drawButton(276, 136, 190, 72, "Z ENDE", sessionFocusEndSet_, canCapture);
    drawGridSummary(225);
    const bool ready = readyToScan(status);
    drawButton(116, 246, 300, 62, "SCAN STARTEN", false, ready);
}

void ScanUi::drawGridSummary(int16_t y) {
    const String xCount = columns_ > 0 ? String(columns_) : "--";
    const String yCount = rows_ > 0 ? String(rows_) : "--";
    const String total = totalImages_ > 0 ? String(totalImages_) : "--";
    String summary = "X: " + xCount + "   Y: " + yCount + "   Z: " + String(profile_.focusSteps) +
        "   GESAMT: " + total;
    if (totalImages_ > 0 && (!uniformX_ || !uniformY_)) {
        summary += "   RAND:";
        if (!uniformX_) summary += "X";
        if (!uniformY_) summary += "Y";
    }
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(totalImages_ > 0 ? text : muted, background);
    display_.setFreeFont(&FreeSansBold9pt7b);
    display_.drawString(summary, 266, y);
    display_.setTextFont(1);
}

void ScanUi::drawProgress() {
    drawHeader("SCAN LAEUFT");
    const int completed = min(currentIndex_, totalImages_);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(phase_ == Phase::Error ? red : cyan, background);
    drawUiText(display_, phaseLabel(static_cast<int>(phase_)), 266, 76, true);
    display_.setTextColor(text, background);
    drawUiText(display_, String(completed) + " / " + totalImages_, 266, 118, true);
    drawGridSummary(146);
    display_.fillRoundRect(78, 174, 376, 14, 7, lineColor);
    display_.drawRoundRect(77, 173, 378, 16, 8, text);
    const int16_t width = totalImages_ ? 376 * completed / totalImages_ : 0;
    if (width) display_.fillRoundRect(78, 174, width, 14, 7, phase_ == Phase::Done ? green : cyan);
    if (phase_ == Phase::Done || phase_ == Phase::Error) {
        drawButton(156, 242, 220, 50, "SCHLIESSEN", phase_ == Phase::Done);
    } else if (phase_ == Phase::Recover) {
        drawButton(156, 242, 220, 50, "STOPPE...", false, false);
    } else {
        drawButton(76, 236, 178, 56, phase_ == Phase::Paused || pauseRequested_ ? "FORTSETZEN" : "PAUSE", phase_ == Phase::Paused || pauseRequested_);
        drawButton(278, 236, 178, 56, "STOP", true);
    }
}

void ScanUi::drawSettingsMenu() {
    drawHeader("EINSTELLUNGEN");
    drawButton(68, 54, 190, 46, "BILDFELD", profile_.frameXSet && profile_.frameYSet);
    drawButton(274, 54, 190, 46, "OVERLAP " + String(profile_.overlapMin) + "-" + String(profile_.overlapMax) + "%");
    drawButton(68, 108, 190, 46, "TEMPO XY/Z");
    drawButton(274, 108, 190, 46, "RUHE " + String(profile_.settleMs));
    drawButton(68, 162, 190, 46, "SCHAERFE " + String(profile_.focusSteps));
    drawButton(274, 162, 190, 46, "KAMERA", profile_.cameraEnabled);
    drawButton(68, 216, 190, 46, "AUFLOESUNG");
    drawButton(274, 216, 190, 46, profile_.returnToStart ? "RUECKKEHR AN" : "RUECKKEHR AUS", profile_.returnToStart);
    drawButton(171, 270, 190, 42, "SCAN-HISTORIE", history_.count() > 0);
}

void ScanUi::drawOverlapMenu() {
    drawHeader("OVERLAP-BEREICH", true);
    drawButton(86, 82, 360, 76, "MINIMUM  " + String(profile_.overlapMin) + "%");
    drawButton(86, 184, 360, 76, "MAXIMUM  " + String(profile_.overlapMax) + "%");
}

void ScanUi::drawSpeedMenu() {
    drawHeader("SCAN-TEMPO", true);
    drawButton(66, 82, 190, 150, "XY  " + String(profile_.speed));
    drawButton(276, 82, 190, 150, "Z  " + String(profile_.zSpeed));
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(muted, background);
    display_.setFreeFont(&FreeSansBold9pt7b);
    display_.drawString("mm/min", 266, 266);
    display_.setTextFont(1);
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
    const char* title = parameter_ == Parameter::OverlapMin ? "OVERLAP MIN" :
        (parameter_ == Parameter::OverlapMax ? "OVERLAP MAX" : parameter_ == Parameter::Speed ? "XY-TEMPO" :
        (parameter_ == Parameter::ZSpeed ? "Z-TEMPO" :
        (parameter_ == Parameter::Settle ? "RUHEZEIT" : "SCHAERFESCHRITTE")));
    drawHeader(title, true);
    drawParameterControl();
    drawButton(70, 210, 120, 70, "-", false); drawButton(342, 210, 120, 70, "+", false);
}

void ScanUi::drawParameterControl() {
    display_.fillRect(80, 62, 372, 58, background);
    display_.fillRect(120, 140, 300, 45, background);
    int value, minimum, maximum;
    String unit;
    if (parameter_ == Parameter::OverlapMin) { value = profile_.overlapMin; minimum = 0; maximum = 80; unit = "%"; }
    else if (parameter_ == Parameter::OverlapMax) { value = profile_.overlapMax; minimum = 0; maximum = 80; unit = "%"; }
    else if (parameter_ == Parameter::Speed) { value = profile_.speed; minimum = 1; maximum = scanMaxSpeed; unit = " mm/min"; }
    else if (parameter_ == Parameter::ZSpeed) { value = profile_.zSpeed; minimum = 1; maximum = scanMaxZSpeed; unit = " mm/min"; }
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
    drawUiText(display_, "IO38  /  EIN IMPULS PRO BILD", 266, 68);
    display_.setTextColor(profile_.cameraEnabled ? green : muted, background);
    drawUiText(display_, profile_.cameraEnabled ? "IO38 AKTIV" : "IO38 INAKTIV", 266, 100, true);
    drawButton(86, 132, 360, 48, profile_.cameraEnabled ? "KAMERA: AN" : "KAMERA: AUS", profile_.cameraEnabled);
    drawButton(86, 190, 360, 48, "IMPULS FEST  15 ms", false, false);
    drawButton(86, 248, 360, 54, cameraTestActive_ ? "IMPULS AKTIV" : "TESTIMPULS", cameraTestActive_, profile_.cameraEnabled);
}

void ScanUi::drawResolution() {
    drawHeader("AUFLOESUNG", true);
    const int widths[] = {1920, 3840, 5472}; const int heights[] = {1080, 2160, 3648};
    for (int i = 0; i < 3; ++i) drawButton(86, 70 + i * 72, 360, 56, String(widths[i]) + " x " + heights[i] + " px", profile_.cameraWidth == widths[i]);
}

void ScanUi::drawHistoryList() {
    drawHeader("SCAN-HISTORIE", true);
    const uint8_t count = history_.count();
    if (count == 0) {
        display_.setTextDatum(MC_DATUM);
        display_.setTextColor(muted, background);
        drawUiText(display_, "NOCH KEINE BEENDETEN SCANS", 266, 164, true);
        return;
    }
    const uint8_t pageCount = (count + historyRowsPerPage - 1) / historyRowsPerPage;
    if (historyPage_ >= pageCount) historyPage_ = pageCount - 1;
    for (uint8_t row = 0; row < historyRowsPerPage; ++row) {
        const uint8_t index = historyPage_ * historyRowsPerPage + row;
        if (index >= count) break;
        ScanHistoryRecord record;
        if (!history_.newest(index, record)) continue;
        const int16_t y = 54 + row * 52;
        display_.fillRoundRect(66, y, 400, 46, 6, raised);
        display_.drawRoundRect(66, y, 400, 46, 6, lineColor);
        display_.drawRoundRect(67, y + 1, 398, 44, 5, lineColor);
        display_.setTextDatum(MC_DATUM);
        display_.setTextColor(cyan, raised);
        display_.setFreeFont(&FreeSansBold9pt7b);
        const String label = "#" + String(record.sequence) + "   " + String(record.columns) + "x" +
            String(record.rows) + "x" + String(record.focusSteps) + "   " + String(record.totalImages) +
            " BILDER   " + formatDuration(record.durationMs);
        display_.drawString(label, 266, y + 23);
        display_.setTextFont(1);
    }
    drawButton(66, 270, 100, 42, "<", false, historyPage_ > 0);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(text, background);
    display_.setFreeFont(&FreeSansBold9pt7b);
    display_.drawString(String(historyPage_ + 1) + " / " + String(pageCount), 266, 291);
    display_.setTextFont(1);
    drawButton(366, 270, 100, 42, ">", false, historyPage_ + 1 < pageCount);
}

void ScanUi::drawHistoryDetail() {
    drawHeader((String("SCAN #") + String(selectedHistory_.sequence)).c_str(), true);
    String lines[10];
    int lineCount = 0;
    if (historyDetailPage_ == 0) {
        lines[lineCount++] = "DAUER  " + formatDuration(selectedHistory_.durationMs);
        lines[lineCount++] = "RASTER  X " + String(selectedHistory_.columns) + "  Y " + String(selectedHistory_.rows) +
            "  Z " + String(selectedHistory_.focusSteps);
        lines[lineCount++] = "BILDER  " + String(selectedHistory_.totalImages);
        lines[lineCount++] = "START  X " + String(selectedHistory_.startX, 3) + "  Y " + String(selectedHistory_.startY, 3);
        lines[lineCount++] = "ENDE   X " + String(selectedHistory_.endX, 3) + "  Y " + String(selectedHistory_.endY, 3);
        lines[lineCount++] = "Z  START " + String(selectedHistory_.startZ, 4) + "  ENDE " + String(selectedHistory_.endZ, 4);
        lines[lineCount++] = "BEREICH  X " + String(fabsf(selectedHistory_.endX - selectedHistory_.startX), 3) +
            "  Y " + String(fabsf(selectedHistory_.endY - selectedHistory_.startY), 3);
        lines[lineCount++] = "BEREICH Z  " + String(fabsf(selectedHistory_.endZ - selectedHistory_.startZ), 4) + " mm";
        lines[lineCount++] = "BILDFELD  X " + String(selectedHistory_.frameX, 4) + "  Y " + String(selectedHistory_.frameY, 4);
    } else {
        lines[lineCount++] = "SCHRITT  X " + String(selectedHistory_.stepX, 4) + "  Y " + String(selectedHistory_.stepY, 4);
        lines[lineCount++] = "SCHRITT Z  " + String(selectedHistory_.stepZ, 4) + " mm";
        lines[lineCount++] = "OVERLAP SOLL  " + String(selectedHistory_.overlapMin) + "-" + String(selectedHistory_.overlapMax) + "%";
        lines[lineCount++] = "OVERLAP IST  X " + String(selectedHistory_.actualOverlapX, 1) +
            "%  Y " + String(selectedHistory_.actualOverlapY, 1) + "%";
        lines[lineCount++] = "TEMPO  XY " + String(selectedHistory_.speed) + "  Z " + String(selectedHistory_.zSpeed) + " mm/min";
        lines[lineCount++] = "RUHE " + String(selectedHistory_.settleMs) + " ms   IMPULS " +
            String(selectedHistory_.cameraPulseMs) + " ms";
        lines[lineCount++] = "KAMERA-PAUSE  " + String(selectedHistory_.cameraRecoveryMs) + " ms";
        lines[lineCount++] = "KAMERA  " + String(selectedHistory_.flags & ScanHistoryStore::cameraEnabledFlag ? "AN" : "AUS");
        lines[lineCount++] = "AUFLOESUNG  " + String(selectedHistory_.cameraWidth) + "x" + String(selectedHistory_.cameraHeight);
        lines[lineCount++] = "RUECKKEHR  " + String(selectedHistory_.flags & ScanHistoryStore::returnToStartFlag ? "AN" : "AUS") +
            "   RAND  " + String((selectedHistory_.flags & ScanHistoryStore::uniformXFlag) ? "-" : "X") +
            String((selectedHistory_.flags & ScanHistoryStore::uniformYFlag) ? "" : "Y");
    }
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(text, background);
    display_.setFreeFont(&FreeSansBold9pt7b);
    for (int index = 0; index < lineCount; ++index) display_.drawString(lines[index], 266, 56 + index * 22);
    display_.setTextFont(1);
    drawButton(66, 270, 100, 42, "<", false, historyDetailPage_ > 0);
    display_.setTextDatum(MC_DATUM);
    display_.setTextColor(text, background);
    display_.setFreeFont(&FreeSansBold9pt7b);
    display_.drawString(String(historyDetailPage_ + 1) + " / 2", 266, 291);
    display_.setTextFont(1);
    drawButton(366, 270, 100, 42, ">", false, historyDetailPage_ < 1);
}

void ScanUi::draw() {
    if (!visible_) return;
    display_.fillRect(left, 0, 480 - left, 320, background);
    if (screen_ == Screen::Workflow) phase_ == Phase::Idle ? drawWorkflow() : drawProgress();
    else if (screen_ == Screen::SettingsMenu) drawSettingsMenu();
    else if (screen_ == Screen::OverlapMenu) drawOverlapMenu();
    else if (screen_ == Screen::SpeedMenu) drawSpeedMenu();
    else if (screen_ == Screen::FieldMenu) drawFieldMenu();
    else if (screen_ == Screen::CalibrateX) drawCalibration('X');
    else if (screen_ == Screen::CalibrateY) drawCalibration('Y');
    else if (screen_ == Screen::Parameter) drawParameter();
    else if (screen_ == Screen::Camera) drawCamera();
    else if (screen_ == Screen::Resolution) drawResolution();
    else if (screen_ == Screen::HistoryList) drawHistoryList();
    else drawHistoryDetail();
}

void ScanUi::redraw() {
    if (!visible_) return;
    display_.fillRect(left, 0, 480 - left, 320, background);
    draw();
}

void ScanUi::openParameter(Parameter value) { parameter_ = value; screen_ = Screen::Parameter; redraw(); }

void ScanUi::startJog(JogDirection direction, char axis) {
    if (!machine_.connected) return;
    jogCancelPending_ = false;
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
    command += jogDirection_ == JogDirection::Positive ? "-0.12" : "0.12";
    command += " F60\r";
    controller_.write(reinterpret_cast<const uint8_t*>(command.c_str()), command.length());
    controller_.flush();
    lastJogAt_ = millis();
}

void ScanUi::stopJog() {
    controller_.write(jogCancel);
    controller_.flush();
    jogDirection_ = JogDirection::None;
    jogCancelPending_ = true;
    lastJogCancelAt_ = jogCancelStartedAt_ = millis();
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
    const int previous = parameter_ == Parameter::OverlapMin ? profile_.overlapMin :
        (parameter_ == Parameter::OverlapMax ? profile_.overlapMax :
        (parameter_ == Parameter::Speed ? profile_.speed :
        (parameter_ == Parameter::ZSpeed ? profile_.zSpeed :
        (parameter_ == Parameter::Settle ? profile_.settleMs : profile_.focusSteps))));
    if (parameter_ == Parameter::OverlapMin) {
        profile_.overlapMin = map(x, sliderLeft, sliderRight, 0, 80);
        if (profile_.overlapMin > profile_.overlapMax) profile_.overlapMax = profile_.overlapMin;
    } else if (parameter_ == Parameter::OverlapMax) {
        profile_.overlapMax = map(x, sliderLeft, sliderRight, 0, 80);
        if (profile_.overlapMax < profile_.overlapMin) profile_.overlapMin = profile_.overlapMax;
    }
    else if (parameter_ == Parameter::Speed) profile_.speed = map(x, sliderLeft, sliderRight, 1, scanMaxSpeed);
    else if (parameter_ == Parameter::ZSpeed) profile_.zSpeed = map(x, sliderLeft, sliderRight, 1, scanMaxZSpeed);
    else if (parameter_ == Parameter::Settle) profile_.settleMs = map(x, sliderLeft, sliderRight, 0, 40) * 50;
    else profile_.focusSteps = map(x, sliderLeft, sliderRight, 1, maxFocusSteps);
    const int current = parameter_ == Parameter::OverlapMin ? profile_.overlapMin :
        (parameter_ == Parameter::OverlapMax ? profile_.overlapMax :
        (parameter_ == Parameter::Speed ? profile_.speed :
        (parameter_ == Parameter::ZSpeed ? profile_.zSpeed :
        (parameter_ == Parameter::Settle ? profile_.settleMs : profile_.focusSteps))));
    if (current == previous) return;
    calculateGrid();
    if (visible_ && screen_ == Screen::Parameter) drawParameterControl();
}

void ScanUi::changeParameter(int delta) {
    if (parameter_ == Parameter::OverlapMin) {
        profile_.overlapMin = constrain(profile_.overlapMin + delta, 0, 80);
        if (profile_.overlapMin > profile_.overlapMax) profile_.overlapMax = profile_.overlapMin;
    } else if (parameter_ == Parameter::OverlapMax) {
        profile_.overlapMax = constrain(profile_.overlapMax + delta, 0, 80);
        if (profile_.overlapMax < profile_.overlapMin) profile_.overlapMin = profile_.overlapMax;
    }
    else if (parameter_ == Parameter::Speed) profile_.speed = constrain(profile_.speed + delta, 1, scanMaxSpeed);
    else if (parameter_ == Parameter::ZSpeed) profile_.zSpeed = constrain(profile_.zSpeed + delta, 1, scanMaxZSpeed);
    else if (parameter_ == Parameter::Settle) profile_.settleMs = constrain(profile_.settleMs + delta * 50, 0, 2000);
    else profile_.focusSteps = constrain(profile_.focusSteps + delta, 1, maxFocusSteps);
    calculateGrid(); saveProfile(); redraw();
}

void ScanUi::calculateGrid() {
    if (!(profile_.frameXSet && profile_.frameYSet && sessionStartSet_ && sessionEndSet_)) {
        columns_ = rows_ = totalImages_ = 0; strideX_ = strideY_ = 0.0F; uniformX_ = uniformY_ = false; return;
    }
    const AxisLayout xLayout = calculateAxisLayout(fabsf(sessionEndX_ - sessionStartX_), profile_.frameX,
                                                   profile_.overlapMin, profile_.overlapMax);
    const AxisLayout yLayout = calculateAxisLayout(fabsf(sessionEndY_ - sessionStartY_), profile_.frameY,
                                                   profile_.overlapMin, profile_.overlapMax);
    columns_ = xLayout.count; rows_ = yLayout.count;
    strideX_ = xLayout.stride; strideY_ = yLayout.stride;
    uniformX_ = xLayout.uniform; uniformY_ = yLayout.uniform;
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

float ScanUi::scanStep(char axis) const {
    if (axis == 'X') {
        if (strideX_ > 0.00001F) return strideX_;
        if (profile_.frameXSet) return profile_.frameX * (1.0F - (profile_.overlapMin + profile_.overlapMax) / 200.0F);
    } else if (axis == 'Y') {
        if (strideY_ > 0.00001F) return strideY_;
        if (profile_.frameYSet) return profile_.frameY * (1.0F - (profile_.overlapMin + profile_.overlapMax) / 200.0F);
    } else if (axis == 'Z' && profile_.focusSteps > 1 && sessionFocusStartSet_ && sessionFocusEndSet_) {
        return fabsf(sessionFocusEndZ_ - sessionFocusStartZ_) / (profile_.focusSteps - 1);
    }
    return 0.0F;
}

float ScanUi::frameDistance(char axis) const {
    if (axis == 'X' && profile_.frameXSet) return profile_.frameX;
    if (axis == 'Y' && profile_.frameYSet) return profile_.frameY;
    if (axis == 'Z' && sessionFocusStartSet_ && sessionFocusEndSet_)
        return fabsf(sessionFocusEndZ_ - sessionFocusStartZ_);
    return 0.0F;
}

float ScanUi::scanRange(char axis) const {
    if (axis == 'X' && sessionStartSet_ && sessionEndSet_) return fabsf(sessionEndX_ - sessionStartX_);
    if (axis == 'Y' && sessionStartSet_ && sessionEndSet_) return fabsf(sessionEndY_ - sessionStartY_);
    if (axis == 'Z' && sessionFocusStartSet_ && sessionFocusEndSet_)
        return fabsf(sessionFocusEndZ_ - sessionFocusStartZ_);
    return 0.0F;
}

ScanOverview ScanUi::overview() const {
    ScanOverview value;
    value.frameX = profile_.frameXSet ? profile_.frameX : 0.0F;
    value.frameY = profile_.frameYSet ? profile_.frameY : 0.0F;
    value.rangeX = scanRange('X'); value.rangeY = scanRange('Y'); value.rangeZ = scanRange('Z');
    value.stepX = scanStep('X'); value.stepY = scanStep('Y'); value.stepZ = scanStep('Z');
    value.actualOverlapX = value.frameX > 0.00001F ? 100.0F * (1.0F - value.stepX / value.frameX) : 0.0F;
    value.actualOverlapY = value.frameY > 0.00001F ? 100.0F * (1.0F - value.stepY / value.frameY) : 0.0F;
    value.overlapMin = profile_.overlapMin; value.overlapMax = profile_.overlapMax;
    value.columns = columns_; value.rows = rows_; value.focusSteps = profile_.focusSteps; value.totalImages = totalImages_;
    value.speed = profile_.speed; value.zSpeed = profile_.zSpeed;
    value.settleMs = profile_.settleMs; value.cameraPulseMs = profile_.cameraPulseMs;
    value.cameraWidth = profile_.cameraWidth; value.cameraHeight = profile_.cameraHeight;
    value.uniformX = uniformX_; value.uniformY = uniformY_;
    value.cameraEnabled = profile_.cameraEnabled; value.returnToStart = profile_.returnToStart;
    return value;
}

bool ScanUi::rangeEndpointAvailable(char axis) const {
    if (axis == 'X' || axis == 'Y') return sessionStartSet_ && sessionEndSet_;
    if (axis == 'Z') return sessionFocusStartSet_ && sessionFocusEndSet_;
    return false;
}

bool ScanUi::moveRangeEndpoint(char axis, bool endPoint, const ScanMachineStatus& machine) {
    if (!controlsAvailable(machine) || !rangeEndpointAvailable(axis)) return false;
    float target = 0.0F;
    if (axis == 'X') target = endPoint ? sessionEndX_ : sessionStartX_;
    else if (axis == 'Y') target = endPoint ? sessionEndY_ : sessionStartY_;
    else if (axis == 'Z') target = endPoint ? sessionFocusEndZ_ : sessionFocusStartZ_;
    else return false;
    const int feed = axis == 'Z' ? profile_.zSpeed : profile_.speed;
    sendLine("G90 G21 G1 " + String(axis) + String(target, 4) + " F" + String(feed));
    return true;
}

bool ScanUi::moveScanStep(char axis, int direction, const ScanMachineStatus& machine) {
    return moveTestDistance(axis, scanStep(axis), direction, machine);
}

bool ScanUi::moveTestDistance(char axis, float distance, int direction, const ScanMachineStatus& machine) {
    if (!controlsAvailable(machine)) return false;
    if (distance <= 0.00001F || (axis != 'X' && axis != 'Y' && axis != 'Z')) return false;
    const int feed = axis == 'Z' ? profile_.zSpeed : profile_.speed;
    sendLine("G91 G21 G1 " + String(axis) + String(direction < 0 ? -distance : distance, 4) + " F" + String(feed));
    return true;
}

void ScanUi::sendLine(const String& value) {
    String line = value + '\r'; controller_.write(reinterpret_cast<const uint8_t*>(line.c_str()), line.length()); controller_.flush();
}

void ScanUi::sendMove(float x, float y, float z) {
    sendLine("G90 G21 G1 X" + String(x, 4) + " Y" + String(y, 4) + " F" + String(profile_.speed));
    sendLine("G90 G21 G1 Z" + String(z, 4) + " F" + String(profile_.zSpeed));
}

void ScanUi::targetForIndex(int index, float& x, float& y, float& z) const {
    const int xyIndex = index / profile_.focusSteps;
    const int focusIndex = index % profile_.focusSteps;
    const int row = xyIndex / columns_; int column = xyIndex % columns_;
    if (row & 1) column = columns_ - 1 - column;
    const float spanX = fabsf(sessionEndX_ - sessionStartX_);
    const float spanY = fabsf(sessionEndY_ - sessionStartY_);
    const float offsetX = min(spanX, column * strideX_);
    const float offsetY = min(spanY, row * strideY_);
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
    scanStartedAt_ = millis(); historyRecorded_ = false;
    releaseTrigger(); phase_ = Phase::SendMove; phaseStartedAt_ = millis(); redraw();
}

void ScanUi::releaseTrigger() { sendLine("M65 P0"); triggerOutputActive_ = false; cameraTestActive_ = false; }

void ScanUi::stopScan() {
    releaseTrigger();
    controller_.write('!'); controller_.write(softReset);
    recoveryUnlockSent_ = false;
    lastRecoveryUnlockAt_ = 0;
    phase_ = Phase::Recover;
    phaseStartedAt_ = millis();
    pauseRequested_ = false;
    redraw();
}

void ScanUi::togglePause() {
    if (phase_ == Phase::Paused) { pauseRequested_ = false; phase_ = Phase::SendMove; phaseStartedAt_ = millis(); }
    else pauseRequested_ = !pauseRequested_;
    redraw();
}

void ScanUi::startCameraTest(const ScanMachineStatus& machine) {
    if (!profile_.cameraEnabled || !machine.connected) return;
    if (cameraTestActive_) return;
    sendLine("M64 P0");
    triggerOutputActive_ = true;
    cameraTestActive_ = true;
    phaseStartedAt_ = millis();
}

void ScanUi::advanceScan() {
    ++currentIndex_;
    if (currentIndex_ >= totalImages_) {
        if (profile_.returnToStart) { sendMove(returnX_, returnY_, returnZ_); phase_ = Phase::ReturnStart; moveObserved_ = false; phaseStartedAt_ = millis(); }
        else completeScan();
    } else phase_ = pauseRequested_ ? Phase::Paused : Phase::SendMove;
    redraw();
}

ScanHistoryRecord ScanUi::makeHistoryRecord() const {
    ScanHistoryRecord record;
    record.durationMs = millis() - scanStartedAt_;
    record.totalImages = totalImages_;
    record.startX = sessionStartX_; record.startY = sessionStartY_;
    record.endX = sessionEndX_; record.endY = sessionEndY_;
    record.startZ = profile_.focusSteps > 1 ? sessionFocusStartZ_ : returnZ_;
    record.endZ = profile_.focusSteps > 1 ? sessionFocusEndZ_ : returnZ_;
    record.frameX = profile_.frameX; record.frameY = profile_.frameY;
    record.stepX = strideX_; record.stepY = strideY_;
    record.stepZ = profile_.focusSteps > 1 ? fabsf(record.endZ - record.startZ) / (profile_.focusSteps - 1) : 0.0F;
    record.actualOverlapX = profile_.frameX > 0.00001F ? 100.0F * (1.0F - strideX_ / profile_.frameX) : 0.0F;
    record.actualOverlapY = profile_.frameY > 0.00001F ? 100.0F * (1.0F - strideY_ / profile_.frameY) : 0.0F;
    record.overlapMin = profile_.overlapMin; record.overlapMax = profile_.overlapMax;
    record.columns = columns_; record.rows = rows_; record.focusSteps = profile_.focusSteps;
    record.speed = profile_.speed; record.zSpeed = profile_.zSpeed; record.settleMs = profile_.settleMs;
    record.cameraPulseMs = profile_.cameraPulseMs;
    record.cameraRecoveryMs = cameraRecoveryMs;
    record.cameraWidth = profile_.cameraWidth; record.cameraHeight = profile_.cameraHeight;
    if (profile_.cameraEnabled) record.flags |= ScanHistoryStore::cameraEnabledFlag;
    if (profile_.returnToStart) record.flags |= ScanHistoryStore::returnToStartFlag;
    if (uniformX_) record.flags |= ScanHistoryStore::uniformXFlag;
    if (uniformY_) record.flags |= ScanHistoryStore::uniformYFlag;
    return record;
}

void ScanUi::completeScan() {
    if (!historyRecorded_) historyRecorded_ = history_.append(makeHistoryRecord());
    phase_ = Phase::Done;
}

void ScanUi::service(const ScanMachineStatus& machine) {
    const uint32_t now = millis();
    machine_ = machine;
    if (jogCancelPending_) {
        if ((machine.motion == ScanMotionState::Idle || machine.motion == ScanMotionState::Blocked) &&
            now - jogCancelStartedAt_ >= jogCancelGuardMs) {
            jogCancelPending_ = false;
        } else if (machine.connected && now - lastJogCancelAt_ >= jogCancelRepeatMs) {
            controller_.write(jogCancel);
            controller_.flush();
            lastJogCancelAt_ = now;
        }
    }
    if (touchAction_ == TouchAction::Jog && now - lastJogAt_ >= 150) sendJogSegment();
    if (cameraTestActive_ && now - phaseStartedAt_ >= static_cast<uint32_t>(profile_.cameraPulseMs)) { releaseTrigger(); if (screen_ == Screen::Camera) redraw(); }
    if (phase_ == Phase::Recover) {
        if (machine.connected && recoveryUnlockSent_ && machine.motion == ScanMotionState::Idle) {
            phase_ = Phase::Error;
            redraw();
            return;
        }
        if (machine.connected && now - phaseStartedAt_ >= 300 &&
            (!recoveryUnlockSent_ || now - lastRecoveryUnlockAt_ >= 500)) {
            sendLine("$X");
            recoveryUnlockSent_ = true;
            lastRecoveryUnlockAt_ = now;
        }
        return;
    }
    if (phase_ == Phase::Idle || phase_ == Phase::Done || phase_ == Phase::Error) return;
    if (!machine.connected || machine.motion == ScanMotionState::Blocked) { stopScan(); return; }
    if (phase_ == Phase::Paused) return;
    if (phase_ == Phase::SendMove) {
        float x, y, z; targetForIndex(currentIndex_, x, y, z); sendMove(x, y, z); moveObserved_ = false; phase_ = Phase::WaitMove; phaseStartedAt_ = now; redraw();
    } else if (phase_ == Phase::WaitMove) {
        if (machine.motion == ScanMotionState::Moving) moveObserved_ = true;
        if (machine.motion == ScanMotionState::Idle && (moveObserved_ || now - phaseStartedAt_ > 350)) { phase_ = Phase::Settle; phaseStartedAt_ = now; redraw(); }
    } else if (phase_ == Phase::Settle && now - phaseStartedAt_ >= static_cast<uint32_t>(profile_.settleMs)) {
        if (profile_.cameraEnabled) {
            sendLine("M64 P0");
            triggerOutputActive_ = true;
            phase_ = Phase::TriggerOn;
            phaseStartedAt_ = millis();
        }
        else advanceScan();
    } else if (phase_ == Phase::TriggerOn && now - phaseStartedAt_ >= static_cast<uint32_t>(profile_.cameraPulseMs)) {
        releaseTrigger(); phase_ = Phase::TriggerOff; phaseStartedAt_ = millis(); redraw();
    } else if (phase_ == Phase::TriggerOff && now - phaseStartedAt_ >= cameraRecoveryMs) advanceScan();
    else if (phase_ == Phase::ReturnStart) {
        if (machine.motion == ScanMotionState::Moving) moveObserved_ = true;
        if (machine.motion == ScanMotionState::Idle && (moveObserved_ || now - phaseStartedAt_ > 350)) { completeScan(); redraw(); }
    }
}

void ScanUi::onPress(int16_t x, int16_t y, const ScanMachineStatus& machine) {
    if (screen_ == Screen::Workflow && phase_ != Phase::Idle) {
        if ((phase_ == Phase::Done || phase_ == Phase::Error) && inside(x, y, 156, 242, 220, 50)) { phase_ = Phase::Idle; redraw(); }
        else if (phase_ != Phase::Done && phase_ != Phase::Error && phase_ != Phase::Recover) {
            if (inside(x, y, 76, 236, 178, 56)) togglePause(); else if (inside(x, y, 278, 236, 178, 56)) stopScan();
        }
        return;
    }
    if (screen_ != Screen::Workflow && screen_ != Screen::SettingsMenu && inside(x, y, 62, 8, 58, 32)) {
        calibrationASet_ = false;
        if (screen_ == Screen::HistoryDetail) screen_ = Screen::HistoryList;
        else if (screen_ == Screen::CalibrateX || screen_ == Screen::CalibrateY) screen_ = Screen::FieldMenu;
        else if (screen_ == Screen::Parameter &&
                 (parameter_ == Parameter::OverlapMin || parameter_ == Parameter::OverlapMax)) screen_ = Screen::OverlapMenu;
        else if (screen_ == Screen::Parameter &&
                 (parameter_ == Parameter::Speed || parameter_ == Parameter::ZSpeed)) screen_ = Screen::SpeedMenu;
        else screen_ = Screen::SettingsMenu;
        redraw(); return;
    }
    if (screen_ == Screen::Workflow) {
        if (inside(x, y, 66, 54, 190, 72)) {
            if (controlsAvailable(machine)) {
                sessionStartX_ = machine.x; sessionStartY_ = machine.y; sessionStartSet_ = true; calculateGrid(); redraw();
            }
        } else if (inside(x, y, 276, 54, 190, 72)) {
            if (controlsAvailable(machine)) {
                sessionEndX_ = machine.x; sessionEndY_ = machine.y; sessionEndSet_ = true; calculateGrid(); redraw();
            }
        } else if (inside(x, y, 66, 136, 190, 72)) {
            if (controlsAvailable(machine)) {
                sessionFocusStartZ_ = machine.z; sessionFocusStartSet_ = true; redraw();
            }
        } else if (inside(x, y, 276, 136, 190, 72)) {
            if (controlsAvailable(machine)) {
                sessionFocusEndZ_ = machine.z; sessionFocusEndSet_ = true; redraw();
            }
        } else if (inside(x, y, 116, 246, 300, 62)) startScan(machine);
    } else if (screen_ == Screen::SettingsMenu) {
        if (inside(x, y, 68, 54, 190, 46)) { screen_ = Screen::FieldMenu; redraw(); }
        else if (inside(x, y, 274, 54, 190, 46)) { screen_ = Screen::OverlapMenu; redraw(); }
        else if (inside(x, y, 68, 108, 190, 46)) { screen_ = Screen::SpeedMenu; redraw(); }
        else if (inside(x, y, 274, 108, 190, 46)) openParameter(Parameter::Settle);
        else if (inside(x, y, 68, 162, 190, 46)) openParameter(Parameter::FocusSteps);
        else if (inside(x, y, 274, 162, 190, 46)) { screen_ = Screen::Camera; redraw(); }
        else if (inside(x, y, 68, 216, 190, 46)) { screen_ = Screen::Resolution; redraw(); }
        else if (inside(x, y, 274, 216, 190, 46)) { profile_.returnToStart = !profile_.returnToStart; saveProfile(); redraw(); }
        else if (inside(x, y, 171, 270, 190, 42)) { historyPage_ = 0; screen_ = Screen::HistoryList; redraw(); }
    } else if (screen_ == Screen::OverlapMenu) {
        if (inside(x, y, 86, 82, 360, 76)) openParameter(Parameter::OverlapMin);
        else if (inside(x, y, 86, 184, 360, 76)) openParameter(Parameter::OverlapMax);
    } else if (screen_ == Screen::SpeedMenu) {
        if (inside(x, y, 66, 82, 190, 150)) openParameter(Parameter::Speed);
        else if (inside(x, y, 276, 82, 190, 150)) openParameter(Parameter::ZSpeed);
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
        if (inside(x, y, 86, 132, 360, 48)) { profile_.cameraEnabled = !profile_.cameraEnabled; if (!profile_.cameraEnabled) releaseTrigger(); saveProfile(); redraw(); }
        else if (inside(x, y, 86, 248, 360, 54)) startCameraTest(machine);
    } else if (screen_ == Screen::Resolution) {
        if (inside(x, y, 86, 70, 360, 56)) { profile_.cameraWidth = 1920; profile_.cameraHeight = 1080; }
        else if (inside(x, y, 86, 142, 360, 56)) { profile_.cameraWidth = 3840; profile_.cameraHeight = 2160; }
        else if (inside(x, y, 86, 214, 360, 56)) { profile_.cameraWidth = 5472; profile_.cameraHeight = 3648; }
        else return;
        saveProfile(); redraw();
    } else if (screen_ == Screen::HistoryList) {
        const uint8_t count = history_.count();
        const uint8_t pageCount = count == 0 ? 0 : (count + historyRowsPerPage - 1) / historyRowsPerPage;
        for (uint8_t row = 0; row < historyRowsPerPage; ++row) {
            const uint8_t index = historyPage_ * historyRowsPerPage + row;
            if (index < count && inside(x, y, 66, 54 + row * 52, 400, 46) && history_.newest(index, selectedHistory_)) {
                historyDetailPage_ = 0; screen_ = Screen::HistoryDetail; redraw(); return;
            }
        }
        if (inside(x, y, 66, 270, 100, 42) && historyPage_ > 0) { --historyPage_; redraw(); }
        else if (inside(x, y, 366, 270, 100, 42) && historyPage_ + 1 < pageCount) { ++historyPage_; redraw(); }
    } else if (screen_ == Screen::HistoryDetail) {
        if (inside(x, y, 66, 270, 100, 42) && historyDetailPage_ > 0) { --historyDetailPage_; redraw(); }
        else if (inside(x, y, 366, 270, 100, 42) && historyDetailPage_ < 1) { ++historyDetailPage_; redraw(); }
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
