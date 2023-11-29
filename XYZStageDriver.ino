#include "MotorDriver.h"
#include "Stitcher.h"
#include "JoyStick.h"
//#include <memory>

const int xFrames = 31;//XSTEPSIZE / XSTEPSPERPICTURE + 1;
const int yFrames = 19;//YSTEPSIZE / YSTEPSPERPICTURE;


//Shutter
const int transistor = 36;



using namespace std;
shared_ptr<MotorDriver> mot_driver;
//Stitcher * stitcher = new Stitcher();
 // auto joystick = new JoyStick();




// void stepDelay(boolean dir, byte dirPin, byte stepperPin, int delayTime)
// {
//   digitalWrite(dirPin, dir);
//   //delay(100);
//   digitalWrite(stepperPin, HIGH);
//   delayMicroseconds(delayTime);
//   digitalWrite(stepperPin, LOW);
//   delayMicroseconds(delayTime);
// }

// void stepNumber(boolean dir, byte dirPin, byte stepperPin, int steps)
// {
//   for (int i=0; i<steps; ++i)
//   {
//     digitalWrite(dirPin, dir);
//     digitalWrite(stepperPin, HIGH);
//     delayMicroseconds(delayTime);
//     digitalWrite(stepperPin, LOW);
//     delayMicroseconds(delayTime);
//   }  
// }

// void manualDrive()
// {
//   boolean xDir, yDir;
//   float xValue, yValue, zValue;

//   int buttonState = 0;

//   delay(1000);
//   while(digitalRead(ButtonPin) != LOW)
//   {
//     buttonState = digitalRead(ButtonPin);
//     Serial.println(buttonState);
  
//     xValue = analogRead(joyX);
//     yValue = analogRead(joyY);
//     zValue = analogRead(joyZ);
    
//     xValue = map(xValue, 0,1023, -512, 512);
//     yValue = map(yValue,0,1023,-512,512);
//     //zValue = map(zValue,0,1023,-512,512);

//     //String st = String(xValue) + " " + String(yValue) + " " + String(buttonState) + " " + String(zValue);
//     //Serial.println(st);
    
//     if (xValue < 0)
//     {
//         xValue = abs(xValue);
//         xDir = true;
//     }
//     else 
//     {
//       xDir = false;
//     }


//     if (yValue < 0)
//     {
//       yValue = abs(yValue);
//       yDir = true;
//     }
//     else 
//     {
//       yDir = false;
//     }

//     int xDelayTime = map(xValue, 0, 512, MaxStepDelay, MinStepDelay);
//     int yDelayTime = map(yValue, 0, 512, MaxStepDelay, MinStepDelay);
// /*
//   Serial.println(xDelayTime);
//   Serial.println("\t");
//   Serial.println(yDelayTime);
//   //Serial.println(micros());
//   */
    

//   //nowTime = micros();
//     if((micros() - xLastTime) > xDelayTime && xValue > noiseGate)
//     {
//       stepDelay(xDir, X_DIR, X_STP, xDelayTime);
//       xLastTime = micros();
//       //Serial.println("x");
//     }

//   //nowTime = micros();
//     if((micros() - yLastTime) > yDelayTime && yValue > noiseGate)
//     {
//       stepDelay(yDir, Y_DIR, Y_STP, yDelayTime);
//       yLastTime = micros();
//     }
//     int now = micros() - xLastTime;
//     //Serial.println(now);
//   }
// }

// void shoot()
// {
//   delay(300);
//   digitalWrite (transistor, HIGH);
//   delay(300);
//   digitalWrite (transistor, LOW);

// }

void setup()
{
  Serial.begin(9600);
  delay(100);

  // - Init I/O Connections
 // pinMode(BUTTON_PIN, INPUT_PULLUP); // ToDO with arduino mega
  pinMode (transistor, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT); 
  pinMode(X_STEP_PIN,OUTPUT);  
  pinMode(Y_DIR_PIN, OUTPUT); 
  pinMode(Y_STEP_PIN,OUTPUT);
  pinMode(EN, OUTPUT);
  digitalWrite(EN,LOW);


  //auto mot_driver = new MotorDriver();
  //shared_ptr<MotorDriver> mot_driver{new MotorDriver()};
  mot_driver = make_shared<MotorDriver>();

 // auto stitcher = new Stitcher();
 // auto joystick = new JoyStick();

  delay (1000);


}

void loop()
{
    //mot_driver->make_step_with_motor(xMotor, 5000, true, 1000);
    mot_driver->make_step_with_motor(xMotor, 5000, true, 1000);
    return 0;

//  mot_driver[m_type.xMotor]->walk(50, -1, 10);
// mot_driver = stitcher->own_the_mot_driver(mot_driver);



  // Serial.println(xFrames);
  // Serial.println(yFrames);
  // manualDrive();
  

  // -------------------- ------------------------ ----------------------- -------------------- //
  // -------------------- ------------------------ ----------------------- -------------------- //

  

//   boolean xDir, yDir;
//   float xValue, yValue;

//   xDir = false;
//   yDir = true;

//   start:
//   Serial.println("Start:");
  
//   delay(1000);
//   int iy = 0;
//   int ix=0;
 
//   if (!done)
//   {
//     for(iy = 0; iy < yFrames; ++iy)
//     {
//       for(ix = 0; ix < xFrames; ++ix)
//       {
//         Serial.println("x:");
//         Serial.println(ix);
        
//         shoot();
//         stepNumber(xDir, X_DIR, X_STP, XSTEPSPERPICTURE);

//         if(digitalRead(ButtonPin) != HIGH)
//         {
//           manualDrive();
//           goto start;
//         }
//       }
//       Serial.println("y:");
//       Serial.println(iy);
//       shoot();
//       stepNumber(yDir, Y_DIR, Y_STP, YSTEPSPERPICTURE);
//       xDir = !xDir;
//     }
    
//     for(ix = 0; ix < xFrames; ++ix)
//     {
//       Serial.println("x:");
//       Serial.println(ix);
//       shoot();
//       stepNumber(xDir, X_DIR, X_STP, XSTEPSPERPICTURE);

     
//     Serial.println(digitalRead(ButtonPin));
    
//       if(digitalRead(ButtonPin) != HIGH)
//       {
//         manualDrive();
//         goto start;
//       }
//     }
//     done = true;
//   }
  
//  shoot();
//  //stepNumber(!xDir, X_DIR, X_STP, XSTEPSIZE);
// // stepNumber(false, Y_DIR, Y_STP, YSTEPSIZE);
    
}
//#endif