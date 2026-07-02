#include "MotorDriver.h"
#include "Scanner.h"
#include "JoyStick.h"
#include <WiFi.h>       // standard library
#include <WebServer.h>  // standard library
#include "WebCode.h"    // .h file that stores your html page code

#define USE_INTRANET

#define LOCAL_SSID "Heimsucht"
#define LOCAL_PASS "DasLebenIstSchoen"

#define AP_SSID "MicroscopeStageController"
#define AP_PASS "BH2"

// variables to store measure data and sensor states
int x_focus_range{ 0 }, y_focus_range{ 0 };
int stack_start_position{ 0 };
std::vector<int> motor_position{ 0, 0, 0 };
std::vector<int> coordinate_base{ 0, 0, 0 };
std::vector<int> measure_start{ 0, 0, 0 };
std::vector<int> measure_end{ 0, 0, 0 };
std::vector<int> measure_diff{ 0, 0, 0 };
std::vector<int> scan_range{ XSCANRANGE, YSCANRANGE, ZSCANRANGE };
std::vector<int> frame_size{ XSTEPSPERPICTURE, YSTEPSPERPICTURE, 0 };

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

bool measure_on = false;
bool timelapse_on = false;
unsigned long timelapse_delay_ms{ 1000 };
unsigned long last_timelapse_shot_ms{ 0 };

std::vector<int> jog_command{ 0, 0, 0 };
std::vector<unsigned long> jog_last_step_us{ 0, 0, 0 };
unsigned long jog_last_command_ms{ 0 };

const unsigned long JOG_TIMEOUT_MS{ 350 };
const int JOG_DEADZONE{ 8 };
const int JOG_MIN_STEP_DELAY_US{ 800 };
const int JOG_MAX_STEP_DELAY_US{ 12000 };
const int MIN_TIMELAPSE_DELAY_MS{ 500 };

using namespace std;
std::shared_ptr<MotorDriver> mot_driver;
std::shared_ptr<Scanner> scanner;

void setup() {
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
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Actual_IP = WiFi.localIP();
#endif

#ifndef USE_INTRANET
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(100);
  WiFi.softAPConfig(PageIP, gateway, subnet);
  delay(100);
  Actual_IP = WiFi.softAPIP();
  Serial.print("IP address: ");
  Serial.println(Actual_IP);
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
  server.on("/B_ADDSTACK", on_button_set_stack_range);
  server.on("/B_RESETSTACK", on_button_reset_stack);
  server.on("/B_STACKSTART", on_button_set_stack_start);
  server.on("/B_RESETBASE", on_button_reset_base);
  server.on("/B_GOBASE", on_button_go_to_base);
  server.on("/B_SCAN", on_button_scan);
  server.on("/B_TLAPSE", on_button_timelapse);
  server.on("/B_JOG", on_button_jog);
  server.on("/B_JOGSTOP", on_button_jog_stop);

  server.begin();

  // - Init I/O Connections
  pinMode(TRANSISTOR, OUTPUT);
  pinMode(X_DIR_PIN, OUTPUT);
  pinMode(X_STEP_PIN, OUTPUT);
  pinMode(Y_DIR_PIN, OUTPUT);
  pinMode(Y_STEP_PIN, OUTPUT);
  pinMode(Z_DIR_PIN, OUTPUT);
  pinMode(Z_STEP_PIN, OUTPUT);

  mot_driver = std::make_shared<MotorDriver>();
  scanner = std::make_shared<Scanner>(mot_driver);
}

void loop() {
  server.handleClient();
  update_live_control();
  update_timelapse();
  update_motor_position();
}

void on_button_scan() {
  scanner->set_focus_range({ x_focus_range, y_focus_range });
  scanner->set_frame_size(frame_size);
  scanner->set_scan_range(scan_range);
  scanner->scan();
  update_motor_position();
}

void on_button_set_frame_size() {
  for (int i = 0; i < measure_diff.size(); ++i)
    frame_size.at(i) = abs(measure_diff.at(i));

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

void on_button_set_stack_range() {
  scanner->set_stack_range(motor_position.at(2) - stack_start_position);

  std::cout << "stacking step " << motor_position.at(2) - stack_start_position << " added" << std::endl;
}

void on_button_reset_stack() {
  scanner->reset_stack();

  std::cout << "reset stacking" << std::endl;
}

void on_button_set_stack_start() {
  stack_start_position = motor_position.at(2);

  std::cout << "set stack start at " << stack_start_position << std::endl;
}

void update_motor_position() {
  motor_position = mot_driver->get_position();

  for (int i = 0; i < motor_position.size(); ++i)
    motor_position.at(i) -= coordinate_base.at(i);
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

void on_button_reset_base() {
  for (int i = 0; i < motor_position.size(); ++i)
    coordinate_base.at(i) += motor_position.at(i);
  // coordinate_base = motor_position;
  update_motor_position();

  std::cout << "new base at"
            << " x = " << coordinate_base.at(0)
            << ", y = " << coordinate_base.at(1)
            << ", z = " << coordinate_base.at(2)
            << std::endl;
}

void on_button_go_to_base() {
  mot_driver->go_to_position(coordinate_base);
}

// same notion for processing button_1
void on_button_measure() {
  if (measure_on) {
    for (int i = 0; i < measure_end.size(); ++i) {
      measure_end.at(i) = motor_position.at(i);
      measure_diff.at(i) = measure_end.at(i) - measure_start.at(i);
    }
    server.send(200, "text/plain", "Measure start");
  } else {
    for (int i = 0; i < measure_end.size(); ++i) {
      measure_start.at(i) = motor_position.at(i);
      measure_diff.at(i) = 0;
    }
    server.send(200, "text/plain", "Measure end");
  }
  measure_on = !measure_on;
}

void on_button_timelapse() {
  if (timelapse_on) {
    timelapse_on = false;
    server.send(200, "text/plain", "Start timelapse");
  } else {
    timelapse_delay_ms = max(server.arg("tl_delay").toInt(), MIN_TIMELAPSE_DELAY_MS);
    last_timelapse_shot_ms = 0;
    timelapse_on = true;
    std::cout << "time lapse delay is: " << timelapse_delay_ms << std::endl;
    server.send(200, "text/plain", "Stop timelapse");
  }
  std::cout << "time lapse is: " << timelapse_on << std::endl;
}

void update_timelapse() {
  if (!timelapse_on)
    return;

  unsigned long now = millis();
  if (last_timelapse_shot_ms == 0 || now - last_timelapse_shot_ms >= timelapse_delay_ms) {
    last_timelapse_shot_ms = now;
    shoot();
  }
}

void shoot() {
  delay(100);
  digitalWrite (TRANSISTOR, HIGH);
  delay(250);
  digitalWrite (TRANSISTOR, LOW);
  std::cout << "shoot();" << std::endl;
}

int clamp_jog_value(int value) {
  if (value > 100)
    return 100;
  if (value < -100)
    return -100;
  return value;
}

void on_button_jog() {
  jog_command.at(0) = clamp_jog_value(server.arg("x").toInt());
  jog_command.at(1) = clamp_jog_value(server.arg("y").toInt());
  jog_command.at(2) = clamp_jog_value(server.arg("z").toInt());
  jog_last_command_ms = millis();
  server.send(200, "text/plain", "OK");
}

void on_button_jog_stop() {
  for (int i = 0; i < jog_command.size(); ++i)
    jog_command.at(i) = 0;

  server.send(200, "text/plain", "OK");
}

void update_live_control() {
  if (millis() - jog_last_command_ms > JOG_TIMEOUT_MS) {
    for (int i = 0; i < jog_command.size(); ++i)
      jog_command.at(i) = 0;
  }

  unsigned long now_us = micros();
  m_type motors[] = { xMotor, yMotor, zMotor };
  bool positive_dirs[] = { XDIR, YDIR, ZDIR };

  for (int i = 0; i < jog_command.size(); ++i) {
    int speed = jog_command.at(i);
    int abs_speed = abs(speed);

    if (abs_speed < JOG_DEADZONE)
      continue;

    int step_delay = map(abs_speed, JOG_DEADZONE, 100, JOG_MAX_STEP_DELAY_US, JOG_MIN_STEP_DELAY_US);

    if (now_us - jog_last_step_us.at(i) >= static_cast<unsigned long>(step_delay * 2)) {
      bool dir = speed > 0 ? positive_dirs[i] : !positive_dirs[i];
      mot_driver->make_step_with_motor(motors[i], 1, dir, step_delay);
      jog_last_step_us.at(i) = micros();
    }
  }
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

  sprintf(buf, "<SS>%d</SS>\n", scanner->get_stack_range());
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
