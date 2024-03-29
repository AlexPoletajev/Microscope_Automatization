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

// variables to store measure data and sensor states
int x_focus_range{0}, y_focus_range{0};
std::vector<int> motor_position{ 0, 0, 0 };
std::vector<int> coordinate_base{ 0, 0, 0 };
std::vector<int> measure_start{ 0, 0, 0 };
std::vector<int> measure_end{ 0, 0, 0 };
std::vector<int> measure_diff{ 0, 0, 0 };
std::vector<int> scan_range{ XSCANRANGE , YSCANRANGE, ZSCANRANGE};
std::vector<int> frame_size{ XSTEPSPERPICTURE, YSTEPSPERPICTURE, 0};

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
  server.on("/B_SETXFOCUS", on_button_set_x_focus_range);
  server.on("/B_SETYFOCUS", on_button_set_y_focus_range);
  server.on("/B_SCANRANGE", on_button_set_scan_range);
  server.on("/B_DRIVEXRANGE", on_button_drive_x_scan_range);
  server.on("/B_DRIVEYRANGE", on_button_drive_y_scan_range);
  server.on("/B_ADDSTACK", on_button_add_stacking_step);
  server.on("/B_RESETSTACK", on_button_reset_stacking);
  server.on("/B_SCAN", on_button_scan);

  server.begin();

  // - Init I/O Connections
  pinMode(TRANSISTOR, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT); 
  pinMode(X_STEP_PIN,OUTPUT);  
  pinMode(Y_DIR_PIN, OUTPUT); 
  pinMode(Y_STEP_PIN,OUTPUT);
  pinMode(Z_DIR_PIN, OUTPUT); 
  pinMode(Z_STEP_PIN,OUTPUT);

  mot_driver = std::make_shared<MotorDriver>();
  scanner = std::make_shared<Scanner>(mot_driver);
}

void loop()
{
   server.handleClient();  
}

void on_button_scan() {
  scanner->set_focus_range( {x_focus_range, y_focus_range } );
  scanner->scan();
}

void on_button_set_frame_size() {
  frame_size = measure_diff;
  scanner->set_frame_size( frame_size );

  std::cout << "set frame size" << std::endl;
}

void on_button_set_x_focus_range() {
  x_focus_range = measure_diff.at(2);

  std::cout << "set z x focus range" << std::endl;
}

void on_button_set_y_focus_range() {
  y_focus_range = measure_diff.at(2);

  std::cout << "set z y focus range" << std::endl;
}

void on_button_set_scan_range() {
  scan_range = measure_diff;
  scanner->set_scan_range( scan_range );

  std::cout << "set scan range" << std::endl;
}

void on_button_drive_x_scan_range() {
  bool dir = false;
  dir = scan_range.at(0) > 0 ? XDIR : !XDIR;
  mot_driver->make_step_with_motor(xMotor, abs(scan_range.at(0)), dir, DELAY);
  // dir = x_focus_range > 0 ? ZDIR : !ZDIR;
  // mot_driver->make_step_with_motor(zMotor, abs(x_focus_range), dir, DELAY);

  std::cout << "drive x scan range" << std::endl;
}

void on_button_drive_y_scan_range() {
  bool dir = false;
  dir = scan_range.at(1) > 0 ? YDIR : !YDIR;
  mot_driver->make_step_with_motor(yMotor, abs(scan_range.at(1)), dir, DELAY);
  //   dir = y_focus_range > 0 ? ZDIR : !ZDIR;
  // mot_driver->make_step_with_motor(zMotor, abs(y_focus_range), dir, DELAY);

  std::cout << "drive y scan range" << std::endl;
}

void on_button_add_stacking_step() {
  scanner->add_stack_step(motor_position.at(2) - measure_start.at(2));

  std::cout << "stacking step " << motor_position.at(2) - measure_start.at(2) << " added" << std::endl;
}

void on_button_reset_stacking() {
  scanner->reset_stack();
  
  std::cout << "reset stacking" << std::endl;
}

void update_motor_position() {
  motor_position = mot_driver->get_position();
}

void on_button_move() {
  int x = server.arg("x_steps").toInt();
  int y = server.arg("y_steps").toInt();
  int z = server.arg("z_steps").toInt();

  bool dir = false;
  dir = x > 0 ? XDIR : !XDIR;
  mot_driver->make_step_with_motor(xMotor, abs(x), dir, DELAY);
  dir = y > 0 ? YDIR : !YDIR;
  mot_driver->make_step_with_motor(yMotor, abs(y), dir, DELAY);
  dir = z > 0 ? ZDIR : !ZDIR;
  mot_driver->make_step_with_motor(zMotor, abs(z), dir, ZDELAY);
  
  update_motor_position();
  server.send(200, "text/plain", "");
}

// same notion for processing button_1
void on_button_measure() {
  if (measure_on)
  {
    measure_end.at(0) = motor_position.at(0);
    measure_diff.at(0) = measure_end.at(0) - measure_start.at(0);
    measure_end.at(1) = motor_position.at(1);
    measure_diff.at(1) = measure_end.at(1) - measure_start.at(1);
    measure_end.at(2) = motor_position.at(2);
    measure_diff.at(2) = measure_end.at(2) - measure_start.at(2);
    server.send(200, "text/plain", "Measure start");
  }
  else
  {
    measure_start.at(0) = motor_position.at(0);
    measure_diff.at(0) = 0;
    measure_start.at(1) = motor_position.at(1);
    measure_diff.at(1) = 0;
    measure_start.at(2) = motor_position.at(2);
    measure_diff.at(2) = 0;
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

  sprintf(buf, "<X0>%d</X0>\n", motor_position.at(0));
  strcat(XML, buf);

  sprintf(buf, "<Y0>%d</Y0>\n", motor_position.at(1));
  strcat(XML, buf);

  sprintf(buf, "<Z0>%d</Z0>\n", motor_position.at(2));
  strcat(XML, buf);

  sprintf(buf, "<XS>%d</XS>\n", measure_start.at(0));
  strcat(XML, buf);

  sprintf(buf, "<XE>%d</XE>\n", measure_end.at(0));
  strcat(XML, buf);

  sprintf(buf, "<XD>%d</XD>\n", measure_diff.at(0));
  strcat(XML, buf);

  sprintf(buf, "<XFS>%d</XFS>\n", frame_size.at(0));
  strcat(XML, buf);

  sprintf(buf, "<XSR>%d</XSR>\n", scan_range.at(0));
  strcat(XML, buf);

  sprintf(buf, "<YS>%d</YS>\n", measure_start.at(1));
  strcat(XML, buf);

  sprintf(buf, "<YE>%d</YE>\n", measure_end.at(1));
  strcat(XML, buf);

  sprintf(buf, "<YD>%d</YD>\n", measure_diff.at(1));
  strcat(XML, buf);

  sprintf(buf, "<YFS>%d</YFS>\n", frame_size.at(1));
  strcat(XML, buf);

  sprintf(buf, "<YSR>%d</YSR>\n", scan_range.at(1));
  strcat(XML, buf);

  sprintf(buf, "<ZS>%d</ZS>\n", measure_start.at(2));
  strcat(XML, buf);

  sprintf(buf, "<ZE>%d</ZE>\n", measure_end.at(2));
  strcat(XML, buf);

  sprintf(buf, "<ZD>%d</ZD>\n", measure_diff.at(2));
  strcat(XML, buf);

  sprintf(buf, "<ZFS>%d</ZFS>\n", frame_size.at(2));
  strcat(XML, buf);

  sprintf(buf, "<ZSR>%d</ZSR>\n", scan_range.at(2));
  strcat(XML, buf);

  sprintf(buf, "<XFR>%d</XFR>\n", x_focus_range);
  strcat(XML, buf);

  sprintf(buf, "<YFR>%d</YFR>\n", y_focus_range);
  strcat(XML, buf);

  sprintf(buf, "<SS>%d</SS>\n", scanner->stack_size());
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