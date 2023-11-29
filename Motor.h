#ifndef _MOTOR_H_
#define _MOTOR_H_

#include <ArduinoSTL.h>
#include <ArxSmartPtr.h>
#include <map>
#include "ControlRoom.h"

class Motor {

  m_type type;

  int id;
  int position{ 0 };
  const byte dir_pin, step_pin;
  int delay_time{ 100 };

  Motor() = default;

  void step();
  
public:

  ~Motor() = default;

  Motor::Motor(const m_type &type, const std::pair<byte, byte> &pin_data)
    //: Motor{ pin_data, Y_DIR_PIN, X_STEP_PIN, Y_STEP_PIN }  //, ID{++count}
    : dir_pin{ pin_data.first }, step_pin{ pin_data.second }, type{ type } {
    //this->type = type;
    std::cout << "Motor with function '" << enum2string(type)
              << "' initialized with dir_pin " << static_cast<int>(dir_pin)
              << " and step_pin: " << static_cast<int>(step_pin) << std::endl;
  }


  void walk(const int &num_steps, const bool &direction, const int &delay_time);
  void go_to_position(const int pos);
  const int get_position(){return &position;};
};





#endif