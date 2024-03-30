#include "Motor.h"

std::map<m_type, std::pair<byte, byte>> motor_data = {
                                                  {xMotor, {X_DIR_PIN, X_STEP_PIN}},
                                                  {yMotor, {Y_DIR_PIN, Y_STEP_PIN}},
                                                  {zMotor, {Z_DIR_PIN, Z_STEP_PIN}}
                                                                                  };

std::string enum2string(m_type t) {

  switch (t) {

    case xMotor: return "X-axis motor";
    case yMotor: return "Y-axis motor";
    case zMotor: return "Z-axis motor";
    default:
      return "Unknown Motor";
  }
}

void Motor::step() {

  digitalWrite(step_pin, HIGH);
  delayMicroseconds(delay_time);
  digitalWrite(step_pin, LOW);
  delayMicroseconds(delay_time);
}

void Motor::walk(const int &num_steps, const bool &direction, const int &delay_time) {

  if (num_steps == 0)
    return;

  digitalWrite(dir_pin, direction);
  this->delay_time = delay_time;

  int position_modifier = (direction == true) ? 1 : -1;

  for (int i = 0; i < abs(num_steps); ++i)
    step();
    
  position += position_modifier * num_steps;
  std::cout<< enum2string(this->type) << " walked "
  << position_modifier * abs(num_steps) << " steps" << std::endl;
}

void Motor::go_to_position(const int pos) {
  int steps_to_go = position - pos;
  bool dir = (steps_to_go > 0) ? true : false;
  walk(steps_to_go, dir, delay_time);
}

