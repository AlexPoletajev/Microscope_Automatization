#include "MotorDriver.h"

MotorDriver::MotorDriver() {
  for (const auto &[type, data] : motor_data) {
    active_motors[type] = new Motor{ type, data };
  }
}
MotorDriver::~MotorDriver() {
  for (auto &[type, ptr] : active_motors)
    delete ptr;
}

MotorDriver::make_step_with_motor(const m_type &motor, const int &num_steps, const bool &direction, const int &delay_time) {
  active_motors[motor]->walk(num_steps, direction, delay_time);
}

std::vector<int> MotorDriver::get_position() {

  std::vector<int> result{ 0, 0, 0 };
  int i{ 0 };
  for (auto [type, ptr] : active_motors) {
    result.at(i++) = ptr->get_position();
  }

  return result;
}
//A498
//int delayTime = 1000;
//int stps= 1500;
//bool done =  false;
