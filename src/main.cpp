#include <Arduino.h>
// #include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106_ESP32.h>
// wifi
#include <wifi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// Button
#include <ezButton.h>

// Buttons
#define BTN_A_PIN 0
#define BTN_B_PIN 2
ezButton button_A(BTN_A_PIN);
ezButton button_B(BTN_B_PIN);
// touchPads 6 in total
const byte touchPad_PIN[] = {27, 14, 12, 13, 15, 4};
int touchedNum = 0; // which pad is touched,0 means none, only 1 pad to touch at a time, if 2 or more pad touched, the lowest number wins

// OLED display
#define OLED_SDA 23 // OLED
#define OLED_SCL 22
Adafruit_SH1106_ESP32 display(OLED_SDA, OLED_SCL);

// Network
const char *ssid = "ssid";
const char *password = "password";
HTTPClient http;
String duet_url = "http://192.168.1.1";

// ArduinoJson
DynamicJsonDocument doc(4096); // Json doc
JsonArray files;               // for cmd

// timer
hw_timer_t *My_timer = NULL;
bool timer_Update = true;

// Menu
struct strut_MODE
{
  int codename;
  String name;
  String url;
  int update_type;
  // 0 for no update requried
  // 1 for only update when change mode
  // 2 for update on timer
  int bottom_type;
  // 0 for no sub menu on bottom,
  // 1 for sub menu of "123456"
  int cmd_state;
  // 0 for close cmd menu
  // 1 for show cmd menu
};
strut_MODE MODE[6] = {
    {0, "Job", "/rr_model?key=job", 2, 0, 0},                           // if WIFI or other Error
    {1, "Status", "/rr_model?flags=d9fn", 2, 0, 0},                     // Status update on timer
    {2, "Main Control", "/rr_files?dir=0:/macros/Main_Control/", 1, 1, 0}, // basic control
    {3, "Heater Control", "/rr_files?dir=0:/macros/Heater_Control/", 1, 1, 0},              // print jobs
    {4, "Fan Control", "/rr_files?dir=0:/macros/Fan_Control/", 1, 1, 0},   
    {5, "Macros", "/rr_files?dir=0:/macros/", 1, 1, 0}};                // macros

static int totalMode = 6;
int currentMode = 1;
int lastMode = 1;
int cmdSelected = 0;
bool cmdRun = false;

String gcode_Run(String url)
{
  url = "/rr_gcode?gcode=M98%20P%22" + url + "%22"; // M98 "path and filename"
  Serial.println(url);

  http.begin(duet_url + url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    // // String jsonStr = http.getString();
    // DeserializationError error = deserializeJson(doc, http.getStream());
    // if (error)
    // {

    //   display.print(F("deserializeJson() failed: "));
    //   display.println(error.f_str());
    // }
    // else
    // {
    //   // all good
    // }
  }
  else
  {
    display.print("Err Code:");
    display.println(httpCode);
  }
  http.end();
  return "Done";
}

void info_Update(int modecode)
{
  String url = MODE[modecode].url;
  http.begin(duet_url + url);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK)
  {
    // String jsonStr = http.getString();
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error)
    {
      display.print(F("deserializeJson() failed: "));
      display.println(error.f_str());
    }
    else
    {
      // all good
    }
  }
  else
  {
    display.print("Err Code:");
    display.println(httpCode);
  }
  http.end();
}
void info_Showtop(int codename)
{
  display.setCursor(0, 0);
  display.print("#");
  display.print(MODE[codename].name);
  display.print("#");
  display.print(touchedNum);
  display.print(MODE[codename].cmd_state);
  display.drawLine(0, 9, 128, 9, WHITE);
  // display.print(codename);
}
void info_Showbottom(int codename)
{
  if (MODE[codename].bottom_type == 1 && MODE[codename].cmd_state == 0)
  {
    display.setCursor(0, 57);
    display.print("1   2   3   4   5   6");
    display.drawLine(0, 55, 128, 55, WHITE);
  }
  if (MODE[codename].bottom_type == 1 && MODE[codename].cmd_state == 1)
  {
    display.setCursor(0, 57);
    display.print("[1]RUN      [6]CANCLE");
    display.drawLine(0, 55, 128, 55, WHITE);
  }
}
void info_Showmain(int codename)
{
  display.setCursor(0, 11);
  if (codename == 0) // null
  {
    JsonObject result = doc["result"];
    JsonObject result_file = result["file"];
    const char *result_file_fileName = result_file["fileName"];
    float result_file_layerHeight = result_file["layerHeight"]; // 0.2
    JsonObject result_timesLeft = result["timesLeft"];
    const char *result_timesLeft_filament = result_timesLeft["filament"];
    if (!result_file["fileName"].isNull())
    {
      display.println(result_file_fileName);
      display.print("Time left: ");
      display.println(result_timesLeft_filament);
    }
    else
    {
      display.print("Not Printing");
    }
  }
  if (codename == 1) // status
  {
    JsonObject result = doc["result"];
    // Status
    JsonObject result_state = result["state"];
    const char *result_state_status = result_state["status"]; // "idle"
    display.println("Status  :" + String(result_state_status));
    // Heaters
    String heatername[2] = {"Tmp Bed :", "Tmp Tool:"};
    int index = 0;
    for (JsonObject result_heat_heater : result["heat"]["heaters"].as<JsonArray>())
    {
      int result_heat_heater_active = result_heat_heater["active"];     // 0, 0
      float result_heat_heater_current = result_heat_heater["current"]; // 21.45, 22.16
      display.print(heatername[index]);
      display.print(result_heat_heater_current);
      display.print("/");
      display.println(result_heat_heater_active);
      index++;
    }
    // Fans
    //  for (JsonObject result_fan : result["fans"].as<JsonArray>())
    //  {
    //    float result_fan_actualValue = result_fan["actualValue"]; // 0, 0
    //  }
    float result_fan_0_actualValue = result["fans"][0]["actualValue"];
    display.print("Fan %   :");
    display.println((int)(result_fan_0_actualValue * 100));
    // Move
    JsonObject result_move = result["move"];
    display.print("X/Y     :");
    index = 0;
    for (JsonObject result_move_axe : result_move["axes"].as<JsonArray>())
    {
      if (index < 2) // print XY as int to save space
      {
        int result_move_axe_machinePosition = result_move_axe["machinePosition"]; // 0, 0, 0
        display.print(result_move_axe_machinePosition);
        display.print("/");
      }
      else // print Z in float
      {
        float result_move_axe_machinePosition = result_move_axe["machinePosition"]; // 0, 0, 0
        display.println();
        display.print("Z       :");
        display.print(result_move_axe_machinePosition);
      }
      index++;
    }
  }

  if (codename >= 2 && codename <=5)
  {
    // JsonArray files;
    const char *dir = doc["dir"]; // "0:/gcodes"
    files = doc["files"];
    int size = files.size();
    // const char *files_1 = files[1]; // "Glasses.gcode"
    if (MODE[codename].cmd_state == 0)
    {
      for (int i = 0; i < min(size, 5); i++)
      {
        const char *file = files[i];
        char fileshort[19];
        strncpy(fileshort, files[i], 18); // too long to display
        fileshort[18] = '\0';
        display.print("[");
        display.print(i + 1);
        display.print("]");
        display.println(fileshort); // "CFFFP_1_Mesh High_P.gcode"
      }
    }
    if (MODE[codename].cmd_state == 1)
    {
      if (cmdSelected > 0) // touchNum is touched
      {
        if (cmdSelected <= files.size())
        {
          const char *file = files[cmdSelected - 1]; // touchNum startering from 1
          display.println("Send CMD:");
          display.print("[");
          //display.print(dir);
          display.print(file);
          display.print("]");

          if (cmdRun == true)
          {
            String url = String(dir) + String(file);
            display.fillRoundRect(20, 20, 128 - 40, 64 - 40, 5, WHITE);
            display.setCursor(40, 27);
            display.setTextColor(BLACK);
            display.print(gcode_Run(url));
            display.setTextColor(WHITE);
            cmdRun = false;
            cmdSelected = 0;
            MODE[codename].cmd_state = 0;
          }
        }
        else
        {
          cmdSelected = 0;
          MODE[codename].cmd_state = 0;
        }
      }
    }
  }
  lastMode = codename;
}
void info_Show(int codename)
{
  // Update from network if necessary
  if (MODE[codename].update_type >= 1)
  {
    if (currentMode != lastMode)
    {
      info_Update(codename);
    }
  }
  if (MODE[codename].update_type == 2)
  {
    if (timer_Update)
    {
      info_Update(codename);
      timer_Update = false;
    }
  }

  // Show info in 3 sections Top Main Bottom
  display.clearDisplay();
  info_Showtop(codename);
  info_Showmain(codename);
  info_Showbottom(codename);
  display.display();
}

void IRAM_ATTR onTimer()
{
  timer_Update = true; // timter flag
}

void button_Update() // cycle modes and close sub menu
{
  if (button_A.isPressed())
  {
    currentMode = (currentMode + totalMode - 1) % totalMode;
    MODE[currentMode].cmd_state = 0;
  }
  if (button_B.isPressed())
  {
    currentMode = (currentMode + 1) % totalMode;
    MODE[currentMode].cmd_state = 0;
  }
}
void touch_Update()
{
  touchedNum = 0;
  // only accept input if sub menu show up, return which pad touched
  if (MODE[currentMode].bottom_type == 1)
  {

    for (int i = 0; i < 6; i++)
    {
      if (touchRead(touchPad_PIN[i]) < 40) // touch threshold 40
      {
        touchedNum = i + 1; // touchNum = 1 if PIN0 touched
        break;
      }
    }
    if (touchedNum > 0)
    {
      if (MODE[currentMode].cmd_state == 0)
      {
        MODE[currentMode].cmd_state = 1;
        cmdSelected = touchedNum;
      }
      else
      {
        if (touchedNum == 1)
        {
          cmdRun = true;
        } // run cmd
        if (touchedNum == 6)
        {
          MODE[currentMode].cmd_state = 0;
          cmdSelected = 0;
        } // cancle cmd
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);
  // initialize with the I2C addr 0x3D (for the 128x64)
  display.begin(SH1106_SWITCHCAPVCC, 0x3C);

  display.clearDisplay();
  display.display();
  display.setTextColor(WHITE, BLACK);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Starting");
  display.display();
  delay(500);

  // Clear the buffer.
  display.clearDisplay();
  // WIFI Connect
  WiFi.begin(ssid, password);
  display.setCursor(0, 0);
  display.print("Connecting");
  // display.sendBuffer();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    display.print(".");
    display.display();
  }
  // display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connected with IP: ");
  display.println(WiFi.localIP());
  display.println("");
  display.println("3D Printer");
  display.display();
  delay(1000);
  display.clearDisplay();

  // HTTPClient settings
  http.useHTTP10(); // need 1.0 to use getStream();
  http.setReuse(true);

  // timer
  My_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(My_timer, &onTimer, true);
  timerAlarmWrite(My_timer, 1000000, true); // 1 sec
  timerAlarmEnable(My_timer);               // Just Enable
}
void loop()
{
  button_A.loop();
  button_B.loop();
  button_Update();
  touch_Update();
  if (WiFi.status() != WL_CONNECTED)
  {
    currentMode = 0;
  }
  info_Show(currentMode);
}
