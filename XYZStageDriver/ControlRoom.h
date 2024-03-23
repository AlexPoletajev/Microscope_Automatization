#ifndef _CONTROL_ROOM_H_
#define _CONTROL_ROOM_H_

// - activate Debuggin e.g. Serial stream output (debug)
#define DEBUG  

// -  Motor
#define X_DIR_PIN         22
#define Y_DIR_PIN         18
#define Z_DIR_PIN         16
#define X_STEP_PIN        23
#define Y_STEP_PIN        19
#define Z_STEP_PIN        17

#define TRANSISTOR         21
#define XDIR               true;
#define YDIR               false;
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

// - Scanning
#define XSTEPSPERPICTURE  860  //1000//968//1291
#define YSTEPSPERPICTURE  300  //441//432//576
#define XSCANRANGE         26100
#define YSCANRANGE         5400  //5300
#define ZSCANRANGE         0  //5300
#define DELAY             2000
#define XFRAMES           0
#define YFRAMES           1

enum m_type { xMotor,
              yMotor,
              zMotor,
              count };

extern std::map<m_type, std::pair<byte, byte>> motor_data;

extern std::string enum2string(m_type t);

#endif