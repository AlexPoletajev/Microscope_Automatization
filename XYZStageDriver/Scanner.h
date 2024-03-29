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
  std::vector<int> scan_range, frame_size, focus_range, start_coordinates, focus_steps_per_frame;
  int transistor_pin{0};
  std::vector<int> stacking_steps;

  // void set_focus_steps_per_frame() {

  // }

public:
  Scanner(std::shared_ptr<MotorDriver> mot_driver) 
  : motor_driver{mot_driver} { transistor_pin = TRANSISTOR; }

  ~Scanner(){};

  void set_scan_range( std::vector<int> range) { scan_range = range; }
  void set_frame_size( std::vector<int> size) { frame_size = size; }
  void set_focus_range( std::vector<int> range) { focus_range = range; std::cout << "set focus range()" << std::endl; }
  void add_stack_step( int value) { stacking_steps.push_back( value ); }
  void reset_stack() { stacking_steps.clear(); }
  int  stack_size() { return stacking_steps.size(); }

  void shoot();
  void shoot_stack();
  void scan();
};

#endif