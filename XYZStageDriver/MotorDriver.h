#ifndef _MOTOR_STITCHER_H_
#define _MOTOR_STITCHER_H_

#include "Motor.h"

class MotorDriver {
  std::map<m_type, Motor *> active_motors;

public:
  MotorDriver();
  ~MotorDriver();

  std::vector<int> get_position();
  void make_step_with_motor(const m_type &motor, const int &num_steps, const bool &direction, const int &delay_time);
  void go_to_position(std::vector<int> position);
};

#endif