#ifndef _MOTOR_STITCHER_H_
#define _MOTOR_STITCHER_H_

#include "Motor.h"

class MotorDriver {

  std::map<m_type, Motor *> active_motors;

public:
  std::vector<int> get_position();
  void make_step_with_motor(const m_type &motor, const int &num_steps, const bool &direction, const int &delay_time);
  MotorDriver();
  ~MotorDriver();

};

#endif