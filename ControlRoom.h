#ifndef _CONTROL_ROOM_H_
#define _CONTROL_ROOM_H_

// - activate Debuggin e.g. Serial stream output (debug)
#define DEBUG  

// -  Motor
#define X_DIR_PIN         5
#define Y_DIR_PIN         7
#define Z_DIR_PIN         9
#define X_STEP_PIN        2
#define Y_STEP_PIN        4
#define Z_STEP_PIN        6
/*
// - Button
#define BUTTON_PIN        A15

// - Joystick
#define X_JOYSTICK_PIN    A8
#define Y_JOYSTICK_PIN    A9
#define Z_JOYSTICK_PIN    A12
*/
#define MAX_STEP_DELAY    10000
#define MIN_STEP_DELAY    20

#define EN 8

// - Stitching
#define XSTEPSPERPICTURE  860  //1000//968//1291
#define YSTEPSPERPICTURE  300  //441//432//576
#define XSTEPSIZE         26100
#define YSTEPSIZE         5400  //5300
#define DELAY             2000
#define XFRAMES           0
#define YFRAMES           1


// - Helper

/*
struct step_vec {
  int x_steps;
  int y_steps;
  int z_steps;

  step_vec(int x, int, y, int, z);
};
*/

enum m_type { xMotor,
              yMotor,
              zMotor,
              count };

extern std::map<m_type, std::pair<byte, byte>> motor_data;

extern std::string enum2string(m_type t);


#endif