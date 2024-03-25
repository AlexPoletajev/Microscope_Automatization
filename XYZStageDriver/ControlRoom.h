#ifndef _CONTROL_ROOM_H_
#define _CONTROL_ROOM_H_

// - activate Debuggin e.g. Serial stream output (debug)
#define DEBUG  

// -  Motor
#define X_DIR_PIN         18
#define Y_DIR_PIN         22
#define Z_DIR_PIN         16
#define X_STEP_PIN        19
#define Y_STEP_PIN        23
#define Z_STEP_PIN        17

#define TRANSISTOR         5
const bool XDIR  =         true;
const bool YDIR  =         true;
const bool ZDIR  =         true;
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
#define XSTEPSPERPICTURE  412
#define YSTEPSPERPICTURE  260
#define XSCANRANGE        6540
#define YSCANRANGE        1320
#define ZSCANRANGE        4000
#define DELAY             2000
#define ZDELAY            500
#define XFRAMES           412
#define YFRAMES           260

enum m_type { xMotor,
              yMotor,
              zMotor,
              count };

extern std::map<m_type, std::pair<byte, byte>> motor_data;

extern std::string enum2string(m_type t);

#endif