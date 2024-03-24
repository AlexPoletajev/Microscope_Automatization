#include "MotorDriver.h"
#include "Scanner.h"
#include "JoyStick.h"
#include <WiFi.h>       // standard library
#include <WebServer.h>  // standard library
#include "WebCode.h"   // .h file that stores your html page code

#define USE_INTRANET

#define LOCAL_SSID "Heimsucht"
#define LOCAL_PASS "DasLebenIstSchoen"

#define AP_SSID "MicroscopeStageController"
#define AP_PASS "BH2"

// start your defines for pins for sensors, outputs etc.
#define PIN_OUTPUT 26 // connected to nothing but an example of a digital write from the web page
#define PIN_FAN 27    // pin 27 and is a PWM signal to control a fan speed
#define PIN_LED 2     //On board LED
#define PIN_A0 34     // some analog input sensor
#define PIN_A1 35     // some analog input sensor

// variables to store measure data and sensor states
int x_mot = 0, z_mot = 0, y_mot = 0;
int x_start = 0, x_end = 0, x_diff = 0, x_scan_range = 0, x_frame_size = 0;
int y_start = 0, y_end = 0, y_diff = 0, y_scan_range = 0, y_frame_size = 0;
int z_start = 0, z_end = 0, z_diff = 0, z_scan_range = 0, z_frame_size = 0;

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

WebServer server(80);

const int xFrames = 31;//XSCANRANGE / XSTEPSPERPICTURE + 1;
const int yFrames = 19;//YSCANRANGE / YSTEPSPERPICTURE;

// sate variable to track measuring procedure
bool measure_on = false;

using namespace std;
std::shared_ptr<MotorDriver> mot_driver;
std::shared_ptr<Scanner> scanner;


void setup()
{
  Serial.begin(9600);
  delay(100);
  disableCore0WDT();

  // maybe disable watch dog timer 1 if needed
  // disableCore1WDT();

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

#ifndef USE_INTRANET
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  WiFi.softAPConfig(PageIP, gateway, subnet);
  delay(100);
  Actual_IP = WiFi.softAPIP();
  Serial.print("IP address: "); Serial.println(Actual_IP);
#endif

  printWifiStatus();

  server.on("/", SendWebsite);
  server.on("/xml", SendXML);

  server.on("/B_MOVE", on_button_move);
  server.on("/B_MEASURE", on_button_measure);
  server.on("/B_SETFRAME", on_button_set_frame_size);
  server.on("/B_SETFOCUS", on_button_set_scan_range);
  server.on("/B_SCAN", on_button_scan);

  server.begin();

  // - Init I/O Connections
 // pinMode(B_PIN, INPUT_PULLUP); // ToDO with arduino mega
  pinMode(TRANSISTOR, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT); 
  pinMode(X_STEP_PIN,OUTPUT);  
  pinMode(Y_DIR_PIN, OUTPUT); 
  pinMode(Y_STEP_PIN,OUTPUT);
  // pinMode(EN, OUTPUT);
  // digitalWrite(EN,LOW);

  x_frame_size = XSTEPSPERPICTURE;
  y_frame_size = YSTEPSPERPICTURE;
  z_frame_size = 0;

  x_scan_range = XSCANRANGE;
  y_scan_range = YSCANRANGE;
  z_scan_range = ZSCANRANGE;

  mot_driver = std::make_shared<MotorDriver>();
  scanner = std::make_shared<Scanner>(mot_driver);
}

void loop()
{
   server.handleClient();  
}

void on_button_scan() {
  scanner->set_scan_range( {x_scan_range, y_scan_range, z_scan_range} );
  scanner->set_frame_size( {x_frame_size, y_frame_size, z_frame_size} );
  scanner->scan();
}

void on_button_set_frame_size() {
  x_frame_size = x_diff;
  y_frame_size = y_diff;
  z_frame_size = z_diff;

  std::cout << "set frame size" << std::endl;
}

void on_button_set_scan_range() {
  x_scan_range = x_diff;
  y_scan_range = y_diff;
  z_scan_range = z_diff;

  std::cout << "set scan range" << std::endl;
}

void update_motor_position() {
  std::vector<int> pos = mot_driver->get_position();
  x_mot = pos.at(0);
  y_mot = pos.at(1);
  z_mot = pos.at(2);
}

void on_button_move() {
  int x = server.arg("x_steps").toInt();
  int y = server.arg("y_steps").toInt();
  int z = server.arg("z_steps").toInt();

  bool dir = false;
  dir = x > 0 ? XDIR : !XDIR;
  mot_driver->make_step_with_motor(xMotor, abs(x), dir, 5000);
  dir = y > 0 ? YDIR : !YDIR;
  mot_driver->make_step_with_motor(yMotor, abs(y), dir, 5000);
  dir = z > 0 ? ZDIR : !ZDIR;
  mot_driver->make_step_with_motor(zMotor, abs(z), dir, 5000);
  
  update_motor_position();
  server.send(200, "text/plain", "");
}

// same notion for processing button_1
void on_button_measure() {
  if (measure_on)
  {
    x_end = x_mot;
    x_diff = x_end - x_start;
    y_end = y_mot;
    y_diff = y_end - y_start;
    z_end = z_mot;
    z_diff = z_end - z_start;
    server.send(200, "text/plain", "Measure start");
  }
  else
  {
    x_start = x_mot;
    x_diff = 0;
    y_start = y_mot;
    y_diff = 0;
    z_start = z_mot;
    z_diff = 0;
    server.send(200, "text/plain", "Measure end");
  }
  measure_on = !measure_on;
}

void SendWebsite() {

  Serial.println("sending web page");
  // you may have to play with this value, big pages need more porcessing time, and hence
  // a longer timeout that 200 ms
  server.send(200, "text/html", PAGE_MAIN);
}

void SendXML() {
  strcpy(XML, "<?xml version = '1.0'?>\n<Data>\n");

  sprintf(buf, "<X0>%d</X0>\n", x_mot);
  strcat(XML, buf);

  sprintf(buf, "<Y0>%d</Y0>\n", y_mot);
  strcat(XML, buf);

  sprintf(buf, "<Z0>%d</Z0>\n", z_mot);
  strcat(XML, buf);

  sprintf(buf, "<XS>%d</XS>\n", x_start);
  strcat(XML, buf);

  sprintf(buf, "<XE>%d</XE>\n", x_end);
  strcat(XML, buf);

  sprintf(buf, "<XD>%d</XD>\n", x_diff);
  strcat(XML, buf);

  sprintf(buf, "<XFS>%d</XFS>\n", x_frame_size);
  strcat(XML, buf);

  sprintf(buf, "<XSR>%d</XSR>\n", x_scan_range);
  strcat(XML, buf);

  sprintf(buf, "<YS>%d</YS>\n", y_start);
  strcat(XML, buf);

  sprintf(buf, "<YE>%d</YE>\n", y_end);
  strcat(XML, buf);

  sprintf(buf, "<YD>%d</YD>\n", y_diff);
  strcat(XML, buf);

  sprintf(buf, "<YFS>%d</YFS>\n", y_frame_size);
  strcat(XML, buf);

  sprintf(buf, "<YSR>%d</YSR>\n", y_scan_range);
  strcat(XML, buf);

  sprintf(buf, "<ZS>%d</ZS>\n", z_start);
  strcat(XML, buf);

  sprintf(buf, "<ZE>%d</ZE>\n", z_end);
  strcat(XML, buf);

  sprintf(buf, "<ZD>%d</ZD>\n", z_diff);
  strcat(XML, buf);

  sprintf(buf, "<ZFS>%d</ZFS>\n", z_frame_size);
  strcat(XML, buf);

  sprintf(buf, "<ZSR>%d</ZSR>\n", z_scan_range);
  strcat(XML, buf);

  strcat(XML, "</Data>\n");

  //Serial.println(XML);

  // you may have to play with this value, big pages need more porcessing time, and hence
  // a longer timeout that 200 ms
  server.send(200, "text/xml", XML);
}

// I think I got this code from the wifi example
void printWifiStatus() {

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  Serial.print("Open http://");
  Serial.println(ip);
}

//#endif