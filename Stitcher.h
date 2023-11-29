#ifndef _STITCHER_H_
#define _STITCHER_H_

#include <map>
#include "MotorDriver.h"

struct camera {
  byte cam_pin;
  int shutter_time_ms;
};


struct joy_stick_data {
  int x_value;
  int y_value;
  int z_value;
};

class Stitcher {

  std::shared_ptr<MotorDriver> motor_driver{ nullptr };

public:

  Stitcher(){};

  std::shared_ptr<MotorDriver> own_the_mot_driver(std::shared_ptr<MotorDriver> mot_driver) {
    motor_driver.swap(mot_driver);

    /* To Do */


    return motor_driver;
  }

  ~Stitcher(){};

  //Stitcher(const )
  std::vector<int> Stitch();
  std::vector<int> measure_steps_between_A_B();
};

#endif