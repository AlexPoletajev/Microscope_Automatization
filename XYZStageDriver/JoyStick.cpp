/*#include <vector>
#include "JoyStick.h"

std::vector<int> JoyStick::manualDrive(std::shared_ptr<MotorDriver> motor_driver)
{ 
  bool xDir, yDir;
  float xValue, yValue, zValue;

  const int noiseGate = 100;
  int xLastTime = 0; int yLastTime = 0; int nowTime = 0;

  int buttonState = 0;

  delay(1000);

  while (digitalRead(BUTTON_PIN) != LOW) {

    buttonState = digitalRead(BUTTON_PIN);
    //Serial.println(buttonState);

    xValue = analogRead(X_JOYSTICK_PIN);
    yValue = analogRead(Y_JOYSTICK_PIN);
    zValue = analogRead(Z_JOYSTICK_PIN);

    xValue = map(xValue, 0, 1023, -512, 512);
    yValue = map(yValue, 0, 1023, -512, 512);

    if (xValue < 0) {
      xValue = abs(xValue);
      xDir = true;
    } else {
      xDir = false;
    }


    if (yValue < 0) {
      yValue = abs(yValue);
      yDir = true;
    } else {
      yDir = false;
    }

    int xDelayTime = map(xValue, 0, 512, MAX_STEP_DELAY, MIN_STEP_DELAY);
    int yDelayTime = map(yValue, 0, 512, MAX_STEP_DELAY, MIN_STEP_DELAY);

    if ((micros() - xLastTime) > xDelayTime && xValue > noiseGate) {
      motor_driver->make_step_with_motor(xMotor, 1, xDir, xDelayTime);
      xLastTime = micros();
    }

    if ((micros() - yLastTime) > yDelayTime && yValue > noiseGate) {
      motor_driver->make_step_with_motor(yMotor, 1, yDir, yDelayTime);
      yLastTime = micros();
    }
    int now = micros() - xLastTime;
  }
}*/