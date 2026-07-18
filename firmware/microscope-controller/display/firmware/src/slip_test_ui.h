#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "scan_ui.h"

class SlipTestUi {
public:
    SlipTestUi(TFT_eSPI& display, HardwareSerial& controller);

    void begin();
    void show();
    void cancelInteraction();
    void handleTouch(bool touched, int16_t x, int16_t y, const ScanMachineStatus& machine);
    void service(const ScanMachineStatus& machine);
    bool running() const;

private:
    enum class Screen { Workflow, Speed, Rounds };
    enum class Phase { Idle, SendMove, WaitMove, Done, Error, Recover };
    enum class TouchAction { None, Slider };

    TFT_eSPI& display_;
    HardwareSerial& controller_;
    Screen screen_ = Screen::Workflow;
    Phase phase_ = Phase::Idle;
    TouchAction touchAction_ = TouchAction::None;
    ScanMachineStatus machine_;
    bool visible_ = false;
    bool touchWasDown_ = false;
    bool pointASet_ = false;
    bool pointBSet_ = false;
    bool moveObserved_ = false;
    bool recoveryUnlockSent_ = false;
    float pointAX_ = 0.0F;
    float pointAY_ = 0.0F;
    float pointAZ_ = 0.0F;
    float pointBX_ = 0.0F;
    float pointBY_ = 0.0F;
    float pointBZ_ = 0.0F;
    int speed_ = 100;
    int rounds_ = 10;
    int targetIndex_ = 0;
    uint32_t phaseStartedAt_ = 0;
    uint32_t lastRecoveryUnlockAt_ = 0;

    void loadSettings();
    void saveSettings();
    void draw();
    void redraw();
    void drawHeader(const char* title, bool back = false);
    void drawButton(int16_t x, int16_t y, int16_t width, int16_t height, const String& label,
                    bool active = false, bool enabled = true);
    void drawWorkflow();
    void drawProgress();
    void drawParameter();
    void drawParameterControl();
    void onPress(int16_t x, int16_t y, const ScanMachineStatus& machine);
    void onDrag(int16_t x);
    void onRelease();
    void updateParameterFromX(int16_t x);
    void changeParameter(int delta);
    bool controlsAvailable(const ScanMachineStatus& machine) const;
    void capturePoint(bool pointA, const ScanMachineStatus& machine);
    void startTest(const ScanMachineStatus& machine);
    void stopTest();
    void closeResult();
    void sendMove(bool pointA);
};
