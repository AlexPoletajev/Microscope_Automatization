#include "MotorDriver.h"
#include "Stitcher.h"
#include "JoyStick.h"
#include <WiFi.h>       // standard library
#include <WebServer.h>  // standard library
#include "SuperMon.h"   // .h file that stores your html page code

// here you post web pages to your homes intranet which will make page debugging easier
// as you just need to refresh the browser as opposed to reconnection to the web server
#define USE_INTRANET

// replace this with your homes intranet connect parameters
#define LOCAL_SSID "Heimsucht"
#define LOCAL_PASS "DasLebenIstSchoen"

// once  you are read to go live these settings are what you client will connect to
#define AP_SSID "MicroscopeStageController"
#define AP_PASS "BH2"

// start your defines for pins for sensors, outputs etc.
#define PIN_OUTPUT 26 // connected to nothing but an example of a digital write from the web page
#define PIN_FAN 27    // pin 27 and is a PWM signal to control a fan speed
#define PIN_LED 2     //On board LED
#define PIN_A0 34     // some analog input sensor
#define PIN_A1 35     // some analog input sensor

// variables to store measure data and sensor states
int BitsA0 = 0, BitsA1 = 0;
float VoltsA0 = 0, VoltsA1 = 0;
int FanSpeed = 0;
bool LED0 = false, SomeOutput = false;
uint32_t SensorUpdate = 0;
int FanRPM = 0;

// the XML array size needs to be bigger that your maximum expected size. 2048 is way too big for this example
char XML[2048];

// just some buffer holder for char operations
char buf[32];

// variable for the IP reported when you connect to your homes intranet (during debug mode)
IPAddress Actual_IP;

// definitions of your desired intranet created by the ESP32
IPAddress PageIP(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress ip;

// gotta create a server
WebServer server(80);
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
  disableCore0WDT();

  // maybe disable watch dog timer 1 if needed
  //  disableCore1WDT();

  // just an update to progress
  Serial.println("starting server");

  // if you have this #define USE_INTRANET,  you will connect to your home intranet, again makes debugging easier
#ifdef USE_INTRANET
  WiFi.begin(LOCAL_SSID, LOCAL_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
  Actual_IP = WiFi.localIP();
#endif

  // if you don't have #define USE_INTRANET, here's where you will creat and access point
  // an intranet with no internet connection. But Clients can connect to your intranet and see
  // the web page you are about to serve up
#ifndef USE_INTRANET
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  WiFi.softAPConfig(PageIP, gateway, subnet);
  delay(100);
  Actual_IP = WiFi.softAPIP();
  Serial.print("IP address: "); Serial.println(Actual_IP);
#endif

  printWifiStatus();


  // these calls will handle data coming back from your web page
  // this one is a page request, upon ESP getting / string the web page will be sent
  server.on("/", SendWebsite);

  // upon esp getting /XML string, ESP will build and send the XML, this is how we refresh
  // just parts of the web page
  server.on("/xml", SendXML);

  // upon ESP getting /UPDATE_SLIDER string, ESP will execute the UpdateSlider function
  // same notion for the following .on calls
  // add as many as you need to process incoming strings from your web page
  // as you can imagine you will need to code some javascript in your web page to send such strings
  // this process will be documented in the SuperMon.h web page code
  server.on("/UPDATE_SLIDER", UpdateSlider);
  server.on("/BUTTON_MOVE", on_button_move);
  server.on("/BUTTON_MEASURE", on_button_measure);

  // finally begin the server
  server.begin();
/*
  // - Init I/O Connections
 // pinMode(BUTTON_PIN, INPUT_PULLUP); // ToDO with arduino mega
  pinMode (transistor, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT); 
  pinMode(X_STEP_PIN,OUTPUT);  
  pinMode(Y_DIR_PIN, OUTPUT); 
  pinMode(Y_STEP_PIN,OUTPUT);
  pinMode(EN, OUTPUT);
  digitalWrite(EN,LOW);
*/

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
    //mot_driver->make_step_with_motor(xMotor, 5000, true, 1000);
//    return 0;
   server.handleClient();
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

void UpdateSlider() {

  /*// many I hate strings, but wifi lib uses them...
  String t_state = server.arg("VALUE");

  // conver the string sent from the web page to an int
  FanSpeed = t_state.toInt();
  Serial.print("UpdateSlider"); Serial.println(FanSpeed);
  // now set the PWM duty cycle
  ledcWrite(0, FanSpeed);


  // YOU MUST SEND SOMETHING BACK TO THE WEB PAGE--BASICALLY TO KEEP IT LIVE

  // option 1: send no information back, but at least keep the page live
  // just send nothing back
  // server.send(200, "text/plain", ""); //Send web page

  // option 2: send something back immediately, maybe a pass/fail indication, maybe a measured value
  // here is how you send data back immediately and NOT through the general XML page update code
  // my simple example guesses at fan speed--ideally measure it and send back real data
  // i avoid strings at all caost, hence all the code to start with "" in the buffer and build a
  // simple piece of data
  FanRPM = map(FanSpeed, 0, 255, 0, 2400);
  strcpy(buf, "");
  sprintf(buf, "%d", FanRPM);
  sprintf(buf, buf);

  // now send it back
  server.send(200, "text/plain", buf); //Send web page
*/
}

// now process button_0 press from the web site. Typical applications are the used on the web client can
// turn on / off a light, a fan, disable something etc
void on_button_move() {

  int x = server.arg("x_steps").toInt();
  int y = server.arg("y_steps").toInt();
  int z = server.arg("z_steps").toInt();


  Serial.print("Button 0"); 
  Serial.println(x); 
  Serial.println(y); 
  Serial.println(z);
  //

/*

  LED0 = !LED0;
  // // digitalWrite(PIN_LED, LED0);
  Serial.print("Button 0 "); Serial.println(LED0);
  // regardless if you want to send stuff back to client or not
  // you must have the send line--as it keeps the page running
  // if you don't want feedback from the MCU--or let the XML manage
  // sending feeback

  // option 1 -- keep page live but dont send any thing
  // here i don't need to send and immediate status, any status
  // like the illumination status will be send in the main XML page update
  // code
  server.send(200, "text/plain", ""); //Send web page

  // option 2 -- keep page live AND send a status
  // if you want to send feed back immediataly
  // note you must have reading code in the java script
  /*
    if (LED0) {
    server.send(200, "text/plain", "1"); //Send web page
    }
    else {
    server.send(200, "text/plain", "0"); //Send web page
    }
  */
}

// same notion for processing button_1
void on_button_measure() {

  // just a simple way to toggle a LED on/off. Much better ways to do this
  Serial.println("Button 1 press");
  SomeOutput = !SomeOutput;

  digitalWrite(PIN_OUTPUT, SomeOutput);
Serial.print("Button 1 "); Serial.println(LED0);
  // regardless if you want to send stuff back to client or not
  // you must have the send line--as it keeps the page running
  // if you don't want feedback from the MCU--or send all data via XML use this method
  // sending feeback
  server.send(200, "text/plain", ""); //Send web page

  // if you want to send feed back immediataly
  // note you must have proper code in the java script to read this data stream
  /*
    if (some_process) {
    server.send(200, "text/plain", "SUCCESS"); //Send web page
    }
    else {
    server.send(200, "text/plain", "FAIL"); //Send web page
    }
  */
}


// code to send the main web page
// PAGE_MAIN is a large char defined in SuperMon.h
void SendWebsite() {

  Serial.println("sending web page");
  // you may have to play with this value, big pages need more porcessing time, and hence
  // a longer timeout that 200 ms
  server.send(200, "text/html", PAGE_MAIN);

}

// code to send the main web page
// I avoid string data types at all cost hence all the char mainipulation code
void SendXML() {

  // Serial.println("sending xml");

  strcpy(XML, "<?xml version = '1.0'?>\n<Data>\n");

  // send bitsA0
  sprintf(buf, "<B0>%d</B0>\n", BitsA0);
  strcat(XML, buf);
  // send Volts0
  sprintf(buf, "<V0>%d.%d</V0>\n", (int) (VoltsA0), abs((int) (VoltsA0 * 10)  - ((int) (VoltsA0) * 10)));
  strcat(XML, buf);

  // send bits1
  sprintf(buf, "<B1>%d</B1>\n", BitsA1);
  strcat(XML, buf);
  // send Volts1
  sprintf(buf, "<V1>%d.%d</V1>\n", (int) (VoltsA1), abs((int) (VoltsA1 * 10)  - ((int) (VoltsA1) * 10)));
  strcat(XML, buf);

  // show led0 status
  if (LED0) {
    strcat(XML, "<LED>1</LED>\n");
  }
  else {
    strcat(XML, "<LED>0</LED>\n");
  }

  if (SomeOutput) {
    strcat(XML, "<SWITCH>1</SWITCH>\n");
  }
  else {
    strcat(XML, "<SWITCH>0</SWITCH>\n");
  }

  strcat(XML, "</Data>\n");
  // wanna see what the XML code looks like?
  // actually print it to the serial monitor and use some text editor to get the size
  // then pad and adjust char XML[2048]; above
 // Serial.println(XML);

  // you may have to play with this value, big pages need more porcessing time, and hence
  // a longer timeout that 200 ms
  server.send(200, "text/xml", XML);


}

// I think I got this code from the wifi example
void printWifiStatus() {

  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("Open http://");
  Serial.println(ip);
}

//#endif