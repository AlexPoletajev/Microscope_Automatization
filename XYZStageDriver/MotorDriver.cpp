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

void MotorDriver::make_step_with_motor(const m_type &motor, const int &num_steps, const bool &direction, const int &delay_time) {
  active_motors[motor]->walk(abs(num_steps), direction, delay_time);
}

std::vector<int> MotorDriver::get_position() {
  std::vector<int> result{ 0, 0, 0 };
  int i{ 0 };
  for (auto [type, ptr] : active_motors) {
    result.at(i++) = ptr->get_position();
  }
  return result;
}

void MotorDriver::go_to_position(std::vector<int> position) 
{
  int steps{0}, i{0};
  bool dir;
  std::vector<bool> direction = { XDIR, YDIR, ZDIR };

  for (auto [type, ptr] : active_motors) {
    steps = position.at(i) - ptr->get_position();

    if (steps != 0)
    {
      dir = steps > 0 ? true : false; //direction.at(i) : !direction.at(i);

      std::cout << enum2string(type) << " is in position:   " << ptr->get_position() << std::endl;
      std::cout << enum2string(type) << " goes to position: " << position.at(i) << std::endl;
      //std::cout << enum2string(type) << " has to go       : " << steps << std::endl;

      ptr->walk( abs(steps), dir, DELAY );
    }
    ++i;
  }
}

