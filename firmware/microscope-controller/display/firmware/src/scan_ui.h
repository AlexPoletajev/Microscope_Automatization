#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

enum class ScanMotionState { Unknown, Idle, Moving, Blocked };

struct ScanMachineStatus {
    bool connected = false;
    ScanMotionState motion = ScanMotionState::Unknown;
    float x = 0.0F;
    float y = 0.0F;
    bool positionValid = false;
};

class ScanUi {
public:
    ScanUi(TFT_eSPI& display, HardwareSerial& controller);

    void begin();
    void draw();
    void handleTouch(bool touched, int16_t x, int16_t y, const ScanMachineStatus& machine);
    void service(const ScanMachineStatus& machine);
    bool running() const;

private:
    enum class View { Frame, Area, Run };
    enum class TouchAction { None, Jog, Slider };
    enum class JogDirection { None, XNeg, XPos, YNeg, YPos };
    enum class Slider { None, Overlap, Speed, Settle };
    enum class Phase { Idle, SendMove, WaitMove, Settle, TriggerOn, TriggerOff, Paused, ReturnStart, Done, Error };

    struct Profile {
        float frameX = 0.0F;
        float frameY = 0.0F;
        float frameXA = 0.0F;
        float frameYA = 0.0F;
        float corner1X = 0.0F;
        float corner1Y = 0.0F;
        float corner2X = 0.0F;
        float corner2Y = 0.0F;
        int cameraWidth = 1920;
        int cameraHeight = 1080;
        int overlap = 15;
        int speed = 60;
        int settleMs = 300;
        bool frameXASet = false;
        bool frameYASet = false;
        bool frameXSet = false;
        bool frameYSet = false;
        bool corner1Set = false;
        bool corner2Set = false;
        bool triggerEnabled = false;
        bool returnToStart = true;
    };

    TFT_eSPI& display_;
    HardwareSerial& controller_;
    Profile profile_;
    View view_ = View::Frame;
    TouchAction touchAction_ = TouchAction::None;
    JogDirection jogDirection_ = JogDirection::None;
    Slider activeSlider_ = Slider::None;
    Phase phase_ = Phase::Idle;
    Phase phaseBeforePause_ = Phase::Idle;
    uint32_t phaseStartedAt_ = 0;
    bool touchWasDown_ = false;
    bool moveObserved_ = false;
    bool stopRequested_ = false;
    bool pauseRequested_ = false;
    bool triggerOutputActive_ = false;
    int columns_ = 0;
    int rows_ = 0;
    int currentIndex_ = 0;
    int totalImages_ = 0;
    float startX_ = 0.0F;
    float startY_ = 0.0F;

    void loadProfile();
    void saveProfile();
    void drawTabs();
    void drawFrameView();
    void drawAreaView();
    void drawRunView();
    void drawProgress();
    void drawCompactDpad();
    void drawButton(int16_t x, int16_t y, int16_t width, int16_t height, const char* label, bool active = false);
    void drawSlider(int16_t y, const char* label, int value, int minimum, int maximum, const char* unit);
    void drawPositionValue(int16_t x, int16_t y, const char* label, float value, bool valid);
    void redrawCurrentView();
    void onPress(int16_t x, int16_t y, const ScanMachineStatus& machine);
    void onDrag(int16_t x, int16_t y);
    void onRelease();
    void switchView(View view);
    JogDirection jogAt(int16_t x, int16_t y) const;
    void startJog(JogDirection direction);
    void stopJog();
    void captureFrameX(const ScanMachineStatus& machine);
    void captureFrameY(const ScanMachineStatus& machine);
    void captureCorner(bool first, const ScanMachineStatus& machine);
    void updateSlider(Slider slider, int16_t x);
    void calculateGrid();
    bool readyToScan(const ScanMachineStatus& machine) const;
    void startScan(const ScanMachineStatus& machine);
    void stopScan();
    void togglePause();
    void sendLine(const String& line);
    void sendMove(float x, float y);
    void targetForIndex(int index, float& x, float& y) const;
    void advanceScan(const ScanMachineStatus& machine);
};
