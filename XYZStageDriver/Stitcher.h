#ifndef _SCANNER_H_
#define _SCANNER_H_

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


class Scanner {

  std::shared_ptr<MotorDriver> motor_driver{ nullptr };
  int focal_range{0};
  std::vector<int> scan_range;
  std::vector<int> frame_size;
  int transistor_pin{0};

public:

  Scanner(std::shared_ptr<MotorDriver> mot_driver, int transistor_pin) 
  : motor_driver{mot_driver}, transistor_pin{transistor_pin} {};

  ~Scanner(){};

  void set_scan_range( std::vector<int> range) { std::cout << "scan_range" << std::endl; scan_range = range; }
  void set_frame_size( std::vector<int> size) { std::cout << "frame_size" << std::endl; frame_size = size; }

  void shoot();
  void scan();

};

#endif