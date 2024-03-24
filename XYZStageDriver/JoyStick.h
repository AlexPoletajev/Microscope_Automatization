#ifndef _JOY_STICK_H_
#define _JOY_STICK_H_

#include "MotorDriver.h"


class JoyStick {
  std::shared_ptr<MotorDriver> motor_driver{ nullptr };

public:
  JoyStick(){};

  std::shared_ptr<MotorDriver> own_the_mot_driver(std::shared_ptr<MotorDriver> mot_driver) {
    motor_driver.swap(mot_driver);

    /* To Do */

    return motor_driver;
  }

  std::vector<int> manualDrive(std::shared_ptr<MotorDriver> motor_driver);

  ~JoyStick(){};
};


#endif