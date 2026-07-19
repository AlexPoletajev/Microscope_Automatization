#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

enum class ScanMotionState { Unknown, Idle, Moving, Blocked };

struct ScanMachineStatus {
    bool connected = false;
    ScanMotionState motion = ScanMotionState::Unknown;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    bool positionValid = false;
};

struct ScanOverview {
    float frameX = 0.0F;
    float frameY = 0.0F;
    float rangeX = 0.0F;
    float rangeY = 0.0F;
    float rangeZ = 0.0F;
    float stepX = 0.0F;
    float stepY = 0.0F;
    float stepZ = 0.0F;
    float actualOverlapX = 0.0F;
    float actualOverlapY = 0.0F;
    int overlapMin = 0;
    int overlapMax = 0;
    int columns = 0;
    int rows = 0;
    int focusSteps = 0;
    int totalImages = 0;
    int speed = 0;
    int zSpeed = 0;
    int settleMs = 0;
    int cameraPulseMs = 0;
    int cameraWidth = 0;
    int cameraHeight = 0;
    bool uniformX = false;
    bool uniformY = false;
    bool cameraEnabled = false;
    bool returnToStart = false;
};

class ScanUi {
public:
    ScanUi(TFT_eSPI& display, HardwareSerial& controller);

    void begin();
    void showWorkflow();
    void showSettings();
    void cancelInteraction();
    void draw();
    void handleTouch(bool touched, int16_t x, int16_t y, const ScanMachineStatus& machine);
    void service(const ScanMachineStatus& machine);
    bool running() const;
    float scanStep(char axis) const;
    float frameDistance(char axis) const;
    float scanRange(char axis) const;
    ScanOverview overview() const;
    bool rangeEndpointAvailable(char axis) const;
    bool moveRangeEndpoint(char axis, bool endPoint, const ScanMachineStatus& machine);
    bool moveScanStep(char axis, int direction, const ScanMachineStatus& machine);
    bool moveTestDistance(char axis, float distance, int direction, const ScanMachineStatus& machine);

private:
    enum class Screen { Workflow, SettingsMenu, OverlapMenu, SpeedMenu, FieldMenu, CalibrateX, CalibrateY, Parameter, Camera, Resolution };
    enum class Parameter { OverlapMin, OverlapMax, Speed, ZSpeed, Settle, FocusSteps };
    enum class TouchAction { None, Jog, Slider };
    enum class JogDirection { None, Negative, Positive };
    enum class Phase { Idle, SendMove, WaitMove, Settle, TriggerOn, TriggerOff, Paused, ReturnStart, Done, Error, Recover };

    struct Profile {
        float frameX = 0.0F;
        float frameY = 0.0F;
        int cameraWidth = 1920;
        int cameraHeight = 1080;
        int overlapMin = 10;
        int overlapMax = 20;
        int speed = 60;
        int zSpeed = 60;
        int settleMs = 300;
        int focusSteps = 1;
        int cameraPulseMs = 15;
        bool frameXSet = false;
        bool frameYSet = false;
        bool cameraEnabled = false;
        bool returnToStart = true;
    };

    TFT_eSPI& display_;
    HardwareSerial& controller_;
    Profile profile_;
    Screen screen_ = Screen::Workflow;
    Parameter parameter_ = Parameter::OverlapMin;
    TouchAction touchAction_ = TouchAction::None;
    JogDirection jogDirection_ = JogDirection::None;
    Phase phase_ = Phase::Idle;
    uint32_t phaseStartedAt_ = 0;
    bool touchWasDown_ = false;
    bool moveObserved_ = false;
    bool pauseRequested_ = false;
    bool triggerOutputActive_ = false;
    bool cameraTestActive_ = false;
    bool recoveryUnlockSent_ = false;
    bool jogCancelPending_ = false;
    bool visible_ = false;
    char jogAxis_ = 'X';
    uint32_t lastJogAt_ = 0;
    uint32_t lastRecoveryUnlockAt_ = 0;
    uint32_t lastJogCancelAt_ = 0;
    uint32_t jogCancelStartedAt_ = 0;
    ScanMachineStatus machine_;
    float calibrationA_ = 0.0F;
    bool calibrationASet_ = false;
    bool sessionStartSet_ = false;
    bool sessionEndSet_ = false;
    bool sessionFocusStartSet_ = false;
    bool sessionFocusEndSet_ = false;
    float sessionStartX_ = 0.0F;
    float sessionStartY_ = 0.0F;
    float sessionEndX_ = 0.0F;
    float sessionEndY_ = 0.0F;
    float sessionFocusStartZ_ = 0.0F;
    float sessionFocusEndZ_ = 0.0F;
    int columns_ = 0;
    int rows_ = 0;
    int currentIndex_ = 0;
    int totalImages_ = 0;
    float strideX_ = 0.0F;
    float strideY_ = 0.0F;
    bool uniformX_ = false;
    bool uniformY_ = false;
    float returnX_ = 0.0F;
    float returnY_ = 0.0F;
    float returnZ_ = 0.0F;

    void loadProfile();
    void saveProfile();
    void migrateLegacyProfile();
    void redraw();
    void drawHeader(const char* title, bool back = false);
    void drawButton(int16_t x, int16_t y, int16_t width, int16_t height, const String& label, bool active = false, bool enabled = true);
    void drawWorkflow(const ScanMachineStatus* machine = nullptr);
    void drawProgress();
    void drawGridSummary(int16_t y);
    void drawSettingsMenu();
    void drawOverlapMenu();
    void drawSpeedMenu();
    void drawFieldMenu();
    void drawCalibration(char axis);
    void drawParameter();
    void drawParameterControl();
    void drawCamera();
    void drawResolution();
    void drawAxisArrow(int16_t centerX, bool positive, bool pressed = false, bool enabled = true);
    void onPress(int16_t x, int16_t y, const ScanMachineStatus& machine);
    void onDrag(int16_t x);
    void onRelease();
    void openParameter(Parameter parameter);
    void startJog(JogDirection direction, char axis);
    void sendJogSegment();
    void stopJog();
    void captureCalibration(const ScanMachineStatus& machine, char axis);
    void updateParameterFromX(int16_t x);
    void changeParameter(int delta);
    void calculateGrid();
    bool readyToScan(const ScanMachineStatus& machine) const;
    bool controlsAvailable(const ScanMachineStatus& machine) const;
    void startScan(const ScanMachineStatus& machine);
    void stopScan();
    void togglePause();
    void startCameraTest(const ScanMachineStatus& machine);
    void releaseTrigger();
    void sendLine(const String& line);
    void sendMove(float x, float y, float z);
    void targetForIndex(int index, float& x, float& y, float& z) const;
    void advanceScan();
};
