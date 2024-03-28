#include "Scanner.h"
#include "ControlRoom.h"

void Scanner::shoot()
{
  // delay(300);
  // digitalWrite (transistor_pin, HIGH);
  // delay(300);
  // digitalWrite (transistor_pin, LOW);

  std::cout << "shoot();" << std::endl;
}

bool calc_direction( int value) {
  return value > 0 ? true : false;
}

void Scanner::scan() 
{
  start_coordinates = motor_driver->get_position();

  int x_steps_per_frame = 2 * frame_size.at(0) / 3;
  if (x_steps_per_frame < 1)
    x_steps_per_frame = 1;
  int x_num_frames = scan_range.at(0) / x_steps_per_frame;
  x_steps_per_frame = scan_range.at(0) / x_num_frames;

  int y_steps_per_frame = 2 * frame_size.at(1) / 3;
  if (y_steps_per_frame < 1)
    y_steps_per_frame = 1;
  int y_num_frames = scan_range.at(1) / y_steps_per_frame;
  y_steps_per_frame = scan_range.at(1) / y_num_frames;

  int x_correction{0}, y_correction{0};
  if ( (scan_range.at(0) - x_steps_per_frame * x_num_frames) > (frame_size.at(0) / 6) )
    x_correction = scan_range.at(0) - x_steps_per_frame * x_num_frames;
  if ( (scan_range.at(1) - y_steps_per_frame * y_num_frames) > (frame_size.at(1) / 6) )
    y_correction = scan_range.at(0) - y_steps_per_frame * y_num_frames;

  bool x_direction = XDIR;
  bool y_direction = YDIR;

  y_steps_per_frame = x_steps_per_frame = 200;
  x_num_frames = y_num_frames = 3;

  int z_x_steps_per_frame = focus_range.at(0) / x_num_frames;
  int z_y_steps_per_frame = focus_range.at(1) / y_num_frames;
  bool z_x_direction = calc_direction(z_x_steps_per_frame);
  bool z_y_direction = calc_direction(z_y_steps_per_frame);

  std::cout << "x_steps_per_frame : " << x_steps_per_frame << std::endl;
  std::cout << "x_num_frames : " << x_num_frames << std::endl;
  std::cout << "x_num_frames x x_steps_per_frame: " << x_num_frames * x_steps_per_frame << std::endl;

  std::cout << "y_steps_per_frame : " << y_steps_per_frame << std::endl;
  std::cout << "y_num_frames : " << y_num_frames << std::endl;
  std::cout << "y_num_frames x y_steps_per_frame: " << y_num_frames * y_steps_per_frame << std::endl;

  std::cout << "z_x_steps_per_frame : " << z_x_steps_per_frame << std::endl;
  std::cout << "num_frames x z_x_steps_per_frame: " << x_num_frames * z_x_steps_per_frame << std::endl;
  std::cout << "z_y_steps_per_frame : " << z_y_steps_per_frame << std::endl;
  std::cout << "num_frames x z_y_steps_per_frame: " << y_num_frames * z_y_steps_per_frame << std::endl;

  for (int i = 0; i <= x_num_frames; ++i)
  {
    shoot();
    for(int j = 0; j < y_num_frames; ++j)
    {
      motor_driver->make_step_with_motor(yMotor, y_steps_per_frame, y_direction, DELAY);
      motor_driver->make_step_with_motor(zMotor, z_y_steps_per_frame, z_y_direction, ZDELAY);
      shoot();
    }
    motor_driver->make_step_with_motor(yMotor, y_correction, y_direction, DELAY);
    y_direction = !y_direction;
    z_y_direction = !z_y_direction;

    if(i < x_num_frames)
    {
      motor_driver->make_step_with_motor(xMotor, x_steps_per_frame, x_direction, DELAY);
      motor_driver->make_step_with_motor(yMotor, x_correction, y_direction, DELAY);
      motor_driver->make_step_with_motor(zMotor, z_x_steps_per_frame, z_x_direction, ZDELAY);
    }
  }

  motor_driver->go_to_position(start_coordinates);
  std::cout << "scanning done" << std::endl;
}
