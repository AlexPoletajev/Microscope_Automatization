#include "Scanner.h"
#include "ControlRoom.h"

void Scanner::shoot()
{
  delay(300);
  digitalWrite (transistor_pin, HIGH);
  delay(300);
  digitalWrite (transistor_pin, LOW);

  std::cout << "shoot();" << std::endl;
}

void Scanner::scan() 
{
  int x_steps_per_frame = 2 * frame_size.at(0) / 3;
  if (x_steps_per_frame < 1)
    x_steps_per_frame = 1;
  int x_num_frames = scan_range.at(0) / x_steps_per_frame;

  int y_steps_per_frame = 2 * frame_size.at(1) / 3;
  if (y_steps_per_frame < 1)
    y_steps_per_frame = 1;
  int y_num_frames = scan_range.at(1) / y_steps_per_frame;

  int z_steps_per_frame = scan_range.at(2) / x_num_frames;

  bool x_direction = XDIR;
  bool y_direction = YDIR;

  for (int i = 0; i < x_num_frames; ++i)
  {
    shoot();
    for(int j = 0; j < y_num_frames - 1; ++j)
    {
      motor_driver->make_step_with_motor(yMotor, y_steps_per_frame, y_direction, 5000);
      shoot();
    }
    motor_driver->make_step_with_motor(xMotor, x_steps_per_frame, x_direction, 5000);

    y_direction = !y_direction;
  }
  shoot();
  std::cout << "scan" << std::endl;
  }
