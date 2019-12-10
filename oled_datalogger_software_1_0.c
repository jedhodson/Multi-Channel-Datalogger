/*
  OLED Datalogger 3 Channel System
   Jed Hodson 2015
*/

//Includes
#include <LBattery.h>
//Time and Location
#include <LDateTime.h>
#include <LGPS.h>
//OLED and SD
#include <SPI.h>
#include <SD.h>
#include <FTOLED.h>
//Memory Onboard for Logging
#include <LTask.h>
#include <LFlash.h>
#include <LSD.h>
#include <LStorage.h>
#define Drv LFlash          // use Internal 10M Flash
// #define Drv LSD           // use SD card

//Fonts
#include <fonts/Arial14.h>
#include <fonts/Droid_Sans_24.h>

//OLED and SD Params
const byte pin_cs = 7;
const byte pin_dc = 2;
const byte pin_reset = 3;
const byte pin_sd_cs = 4;
OLED oled(pin_cs, pin_dc, pin_reset);

//GPS and Time Params
datetimeInfo t;
unsigned int rtc;
bool GPS_ACTIVE = FALSE;
bool GPS_SYNCED = FALSE;
int getDay;
int getMonth;
int getYear;
int getHour;
int getMin;
int getSec;

//Other Params
const byte CH1 = A0;
const byte CH2 = A1;
const byte CH3 = 9;
int CH1_RAW = 0; //For calculations on Channel data feed
int CH2_RAW = 0;
int CH3_RAW = 0;
float CH1_CAL = 0;
float CH2_CAL = 0;
float CH3_CAL = 0;
const byte button = 8;
int operationStatus = 0;
int screenCycle = 0;
int buttonCycle = 1;
int lastUpdate = 0;
int lastMinute = 0;
bool lowBattery = FALSE;
long currentmillis = 0;
unsigned long sinceCharge = 0;
const int waitInterval = 5000;

//Messages
#define WELCOME1 F("Multi-Channel \nDatalogger by \nJed Hodson 2015 \n\nPlease Wait...")
#define GPSINSTRUC F("Syncing Time via \nGPS. Please Turn \nDevice Upside-down \nin clear view of sky \nThen wait for Sync")

//OLED Boxes
OLED_TextBox titleScreen(oled, 0, 96, 128, 32);
OLED_TextBox mainScreen(oled, 0, 0, 128, 96);

//Test and Debug Params
bool debug = FALSE; //Enable Debug Mode?
const int interval = 2000;

void setup() {
  //Setup OLED and SD
  oled.begin();
  operationStatus = 3;
  setTitle();
  setBackgrounds();
  mainScreen.println(WELCOME1);
  showDebugInfo();
  delay(interval);

  //Setup all params
  pinMode(button, INPUT_PULLUP);
  pinMode(CH1, INPUT);
  pinMode(CH2, INPUT);
  pinMode(CH3, INPUT);

  //Set up memory
  pinMode(10, OUTPUT);

  // see if the card is present and can be initialized:
  LTask.begin();
  Drv.begin();

  mainScreen.clear();

  Serial.begin(115200); //Begin Serial Regardless of DEBUG state
  if (debug == TRUE) //If debug is enabled, wait until a character is sent before beginging
  {
    while (!Serial.available())
    {
      Serial.println("Send character to begin debug");
      mainScreen.println("Waiting for");
      mainScreen.println("Serial Line");
      delay(2000);
      mainScreen.reset();
    }
    mainScreen.clear();
    delay(100);
    Serial.println("Debug Mode Enabled Beginging Startup...");
    delay(interval);
  }

  Serial.println("Going into GPS Set Time Void...");
  setTimeGPS();

  operationStatus = 0;
  setTitle();

}

void loop()
{
  screenChange();
  lowBatteryWarning();
  runLogger();
}

void setBackgrounds()
{
  titleScreen.setBackgroundColour(BLACK);
  mainScreen.setBackgroundColour(BLACK);
  delay(10);
  titleScreen.setForegroundColour(RED);
  mainScreen.setForegroundColour(RED);
  delay(10);
  oled.selectFont(Arial14);
  delay(10);
}

void setTitle()
{
  oled.selectFont(Droid_Sans_24);
  if (operationStatus == 0)
  {
    titleScreen.clear();
    titleScreen.setForegroundColour(GREEN);
    titleScreen.print("RUNNING  ");
    setBackgrounds();
    delay(10);
  } else if (operationStatus == 1)
  {
    titleScreen.clear();
    titleScreen.setForegroundColour(YELLOW);
    titleScreen.print("ATTENTION ");
    setBackgrounds();
    delay(10);
  } else if (operationStatus == 2)
  {
    titleScreen.clear();
    titleScreen.setForegroundColour(RED);
    titleScreen.print("WARNING ");
    setBackgrounds();
    delay(10);
  }
  else if (operationStatus == 3)
  {
    titleScreen.clear();
    titleScreen.setForegroundColour(BLUE);
    titleScreen.print("   WAIT");
    setBackgrounds();
    delay(10);
  }
  else
  {
    titleScreen.clear();
    titleScreen.setForegroundColour(RED);
    titleScreen.print("UNDEFINED");
    setBackgrounds();
    delay(10);
  }
}

void setTimeGPS()
{
  updateRTC();

  operationStatus = 1; //Adjust Title to suit
  setTitle();

  while (GPS_SYNCED != TRUE)
  {
    //Update RTC
    updateRTC();

    mainScreen.print(GPSINSTRUC);
    mainScreen.reset();
    delay(interval);

    //Turn GPS ON or OFF
    if ((GPS_ACTIVE != TRUE) && (t.year < 2010))
    {
      Serial.println("Powering GPS ON and Syncing");
      //mainScreen.clear();
      //mainScreen.println("Sync Started!");
      LGPS.powerOn();
      GPS_ACTIVE = TRUE;
      delay(interval);
    }

    if ((GPS_ACTIVE != TRUE) && (t.year >= 2015))
    {
      Serial.println("Already Synced");
      //mainScreen.clear();
      //mainScreen.println("Already Synced");
      GPS_SYNCED = TRUE;
      LGPS.powerOff();
      GPS_ACTIVE = FALSE;
      delay(interval);
    }

    if ((GPS_ACTIVE == TRUE) && (t.year >= 2015))
    {
      Serial.println("Synced! Powering OFF GPS. Wait...");
      mainScreen.clear();
      //mainScreen.println("Synced! GPS");
      //mainScreen.println("Powering OFF...");
      LGPS.powerOff();
      GPS_ACTIVE = FALSE;
      GPS_SYNCED = TRUE;
      delay(interval);
    }
    //Report Time Regardless if set or not
    //mainScreen.println("Waiting for GPS...");
    //reportTimeAll();
  }

  //Report Synced Time
  mainScreen.clear();
  mainScreen.println("Device Synced!");
  mainScreen.println("Synced Time: ");
  reportTimeAll();
  delay(interval);
  mainScreen.clear();
}

void reportTimeSerial()
{
  if (debug == TRUE)
  {
    Serial.print("Current GMT: ");
    Serial.print(getDay);
    Serial.print("/");
    Serial.print(getMonth);
    Serial.print("/");
    Serial.print(getYear);
    Serial.print("@");
    Serial.print(getHour);
    Serial.print(":");
    Serial.print(getMin);
    Serial.print(":");
    Serial.println(getSec);
    delay(10);
  }
}

void reportTimeOLED()
{
  String OLEDTimeDate = "";
  OLEDTimeDate += getDay;
  OLEDTimeDate += "/";
  OLEDTimeDate += getMonth;
  OLEDTimeDate += "/";
  OLEDTimeDate += getYear;
  OLEDTimeDate += "@";
  OLEDTimeDate += getHour;
  OLEDTimeDate += ":";
  OLEDTimeDate += getMin;
  OLEDTimeDate += ":";
  OLEDTimeDate += getSec;
  mainScreen.println("Current GMT:");
  mainScreen.println(OLEDTimeDate);

  delay(10);
  //mainScreen.reset();
}

void updateRTC()
{
  LDateTime.getTime(&t); //Get Time
  LDateTime.getRtc(&rtc);
}

void reportTimeAll()
{
  updateRTC();
  getDateTimeVars();
  reportTimeSerial();
  reportTimeOLED();
  mainScreen.reset();
  delay(400);
}

void getDateTimeVars()
{
  getDay = t.day;
  getMonth = t.mon;
  getYear = t.year;
  getHour = t.hour;
  getMin = t.min;
  getSec = t.sec;
  delay(10);
}

void timeSinceLast()
{
  updateRTC();
  getDateTimeVars();
  lastUpdate = (getMin - lastMinute);
  String timeSinceLastMes = "Last Log: ";
  timeSinceLastMes += lastUpdate;
  timeSinceLastMes += " Mins";
  mainScreen.println(timeSinceLastMes);
  Serial.println(timeSinceLastMes);
  delay(10);
}

void pushBatteryLevel()
{
  String batteryLevel = "Battery: ";
  batteryLevel += LBattery.level();
  batteryLevel += "%";
  mainScreen.println(batteryLevel);
  delay(10);
  String batteryCharging = "Charging: ";
  if (LBattery.isCharging() == 0)
  {
    batteryCharging += "NO";
  }
  else if (LBattery.isCharging() == 1)
  {
    batteryCharging += "YES";
  }
  mainScreen.println(batteryCharging);
  Serial.println(batteryCharging);
  delay(10);
  batteryCharging = "";
  batteryLevel = "";
}

void batteryLOWCheck()
{
  if (LBattery.level() > 20)
  {
    lowBattery = FALSE;
  }
  else if ((LBattery.level() <= 20) && (LBattery.isCharging() == 0))
  {
    lowBattery = TRUE;
  }
}

void lowBatteryWarning()
{
  batteryLOWCheck();

  if (lowBattery == TRUE)
  {
    operationStatus = 2;
    setTitle();
    mainScreen.clear();
    pushBatteryWarning();
    while (lowBattery == TRUE)
    {
      batteryLOWCheck();
      pushBatteryWarning();
      mainScreen.reset();
      delay(100);
    }
    delay(10);
  }
  else if (lowBattery == FALSE)
  {
    //operationStatus = 0;
    //setTitle();
    delay(10);

    markBattery();
  }
}

void markBattery()
{
  //Mark Full Recharge Battery
  if ((LBattery.level() == 100) && (LBattery.isCharging() == 1))
  {
    sinceCharge = rtc;
    delay(10);
  }
}

void screenChange()
{
  int buttonPressed = digitalRead(button);
  if (buttonPressed == LOW)
  {
    //When Button is pressed change the screen cycle variable up and set Title
    if (buttonCycle == 0)
    {
      setTitle();
      //Serial.println("Pressed: Cycle 0");
      mainScreen.clear();
      buttonCycle = 1;
      screenCycle = 0;
      delay(20);
    }
    else if (buttonCycle == 1)
    {
      //Serial.println("Pressed: Cycle 1");
      mainScreen.clear();
      buttonCycle = 2;
      screenCycle = 1;
      delay(20);
    }
    else if (buttonCycle == 2)
    {
      //Serial.println("Pressed: Cycle 2");
      mainScreen.clear();
      buttonCycle = 3;
      screenCycle = 2;
      delay(20);
    }
    else if (buttonCycle == 3)
    {
      //Serial.println("Pressed: Cycle 3");
      mainScreen.clear();
      buttonCycle = 4;
      screenCycle = 3;
      delay(20);
    }
    else if (buttonCycle == 4)
    {
      //Serial.println("Pressed: Cycle 4");
      pushBlankScreen();
      mainScreen.clear();
      buttonCycle = 0;
      screenCycle = 4;
      delay(20);
    }
  }

  pushScreenChanges();
}

void pushScreenChanges()
{
  //Check which cycle is in and push screen accordingly
  if (screenCycle == 0)
  {
    //Serial.println("Act: Cycle 0");
    pushScreen0();
    mainScreen.reset();
    delay(10);
  }
  else if (screenCycle == 1)
  {
    //Serial.println("Act: Cycle 1");
    pushScreen1();
    mainScreen.reset();
    delay(10);
  }
  else if (screenCycle == 2)
  {
    //Serial.println("Act: Cycle 2");
    pushScreen2();
    mainScreen.reset();
    delay(10);
  }
  else if (screenCycle == 3)
  {
    //Serial.println("Act: Cycle 3");
    pushScreen3();
    mainScreen.reset();
    delay(10);
  }
  else if (screenCycle == 4)
  {
    //Serial.println("Act: Cycle 4");
    //pushBlankScreen();
    delay(10);
  }
}

void pushScreen0()
{
  //SYSINFO 1
  pushBatteryLevel();
  timeSinceLast();
  if (lowBattery == FALSE)
  {
    mainScreen.println("BATTERY OK");
  }
  reportTimeAll();
}

void pushScreen1()
{
  //SYSINFO 2 - Uptime and last full battery charge
  uptime();
  batteryFullUptime();
  mainScreen.print("GPS Active: ");
  if (GPS_ACTIVE == TRUE)
  {
    mainScreen.println("TRUE");
  }
  else
  {
    mainScreen.println("FALSE");
  }

  mainScreen.print("GPS Synced: ");
  if (GPS_SYNCED == TRUE)
  {
    mainScreen.println("TRUE");
  }
  else
  {
    mainScreen.println("FALSE");
  }
}

void pushScreen2()
{
  calculateAll();
  //Channel 1 Display
  mainScreen.print("CH1 RAW: ");
  mainScreen.println(CH1_RAW);
  mainScreen.print("CH1 CAL: ");
  mainScreen.println(CH1_CAL);
  //Channel 2 Display
  mainScreen.print("CH2 RAW: ");
  mainScreen.println(CH2_RAW);
  mainScreen.print("CH2 CAL: ");
  mainScreen.println(CH2_CAL);
  //Channel 3 Display
  mainScreen.print("CH3 RAW: ");
  mainScreen.println(CH3_RAW);
  mainScreen.print("CH3 CAL: ");
  mainScreen.println(CH3_CAL);
  mainScreen.reset();
  delay(10);
}

void pushScreen3()
{
  //Force Update
  mainScreen.println("Press and Hold");
  mainScreen.println("Button to Log.");
  mainScreen.println("or short press to");
  mainScreen.println("skip.");

  int buttonPressed = digitalRead(button); //Check to See if long press or short press
  if (buttonPressed == LOW)
  {
    delay(500);
    int buttonPressed = digitalRead(button);
    if (buttonPressed == LOW)
    {
      delay(2000);
      int buttonPressed = digitalRead(button);
      if (buttonPressed == LOW)
      {
        //Run Logger
        actLogger();
      }
      else
      {
        screenCycle = 4;
        pushScreenChanges();
      }
    } else
    {
      screenCycle = 4;
      pushScreenChanges();
    }
  }
}

void pushBlankScreen()
{
  mainScreen.clear();
  titleScreen.clear();
  delay(10);
}

void pushBatteryWarning()
{
  mainScreen.println("LOW BATTERY");
  pushBatteryLevel();
  batteryFullUptime();
  //uptime();
  //mainScreen.println("");
  mainScreen.println("Plug in Charger!");
  mainScreen.reset();
  markBattery();
  delay(10);
}

void calculateAll()
{
  CH1_Calculations();
  CH2_Calculations();
  CH3_Calculations();
}

void CH1_Calculations()
{
  CH1_RAW = analogRead(CH1);

  float CH1_VOLTAGE = CH1_RAW * 5.0;
  CH1_VOLTAGE /= 1024.0;

  CH1_CAL = (CH1_VOLTAGE - 0.5) * 100 ;
}

void CH2_Calculations()
{
  CH2_RAW = analogRead(CH2);

  float CH2_VOLTAGE = CH2_RAW * 5.0;
  CH2_VOLTAGE /= 1024.0;

  CH2_CAL = (CH2_VOLTAGE - 0.5) * 100 ;
}

void CH3_Calculations()
{
  CH3_RAW = digitalRead(CH3);

  CH3_CAL != CH3_RAW; //Invert Input for calculation
}

void showDebugInfo()
{
  if (debug == TRUE)
  {
    mainScreen.println("DEBUG: TRUE");
  }
  else
  {
    mainScreen.println("DEBUG: FALSE");
  }
}

void uptime()
{
  long days = 0;
  long hours = 0;
  long mins = 0;
  long secs = 0;
  currentmillis = millis();

  secs = currentmillis / 1000; //convect milliseconds to seconds
  mins = secs / 60; //convert seconds to minutes
  hours = mins / 60; //convert minutes to hours
  days = hours / 24; //convert hours to days
  secs = secs - (mins * 60); //subtract the coverted seconds to minutes in order to display 59 secs max
  mins = mins - (hours * 60); //subtract the coverted minutes to hours in order to display 59 minutes max
  hours = hours - (days * 24); //subtract the coverted hours to days in order to display 23 hours max
  //Create String to Display
  String uptimeData = "Uptime: ";
  if (days > 0)
  {
    uptimeData += days;
    uptimeData += "d ";
  }
  uptimeData += hours;
  uptimeData += "h ";
  uptimeData += mins;
  uptimeData += "m ";
  uptimeData += secs;
  uptimeData += "s";

  //Display string on OLED and Serial
  mainScreen.println(uptimeData);
  Serial.println(uptimeData);
  delay(10);
}

void batteryFullUptime()
{
  //Calculate time since battery was last fully charged
  long days1 = 0;
  long hours1 = 0;
  long mins1 = 0;
  long secs1 = 0;
  long cacuSec1 = 0;
  updateRTC();

  cacuSec1 = rtc - sinceCharge;

  secs1 = cacuSec1; //convect milliseconds to seconds
  mins1 = secs1 / 60; //convert seconds to minutes
  hours1 = mins1 / 60; //convert minutes to hours
  days1 = hours1 / 24; //convert hours to days
  secs1 = secs1 - (mins1 * 60); //subtract the coverted seconds to minutes in order to display 59 secs max
  mins1 = mins1 - (hours1 * 60); //subtract the coverted minutes to hours in order to display 59 minutes max
  hours1 = hours1 - (days1 * 24); //subtract the coverted hours to days in order to display 23 hours max
  //Create String to Display
  mainScreen.println("Since Recharge:");
  String uptimeData = "";
  if ((days1 > 0) && (days1 < 16600))
  {
    uptimeData += days1;
    uptimeData += "d ";
  }
  else if (days1 >= 16600)
  {
    uptimeData += "N/A";
  }
  else
  {
    uptimeData += hours1;
    uptimeData += "h ";
    uptimeData += mins1;
    uptimeData += "m ";
    uptimeData += secs1;
    uptimeData += "s";

    //Display string on OLED and Serial
    mainScreen.println(uptimeData);
    Serial.println(uptimeData);
    delay(10);
  }
}

void runLogger()
{
  updateRTC();
  getDateTimeVars();
  //Enter conditions for logging to automatically begin
  if ((getMin == 00) || (getMin == 30) && (lastMinute != getMin))
  {
    actLogger();
  }
}

void actLogger()
{
  updateRTC();
  getDateTimeVars();
  operationStatus = 1;
  setTitle();
  mainScreen.clear();
  mainScreen.println("Logging Data...");
  mainScreen.println("Do NOT power off");
  mainScreen.println("or Disconnect");
  delay(interval);

  calculateAll();

  makeDataString();
  
  delay(waitInterval);
  
  calculateAll();
  makeDataString();
  
  mainScreen.clear();  
  operationStatus = 0;
  setTitle();
}

void makeDataString()
{
  //Make String - Make the CSV File
  String datalogWrite = "";
  datalogWrite += getDay;
  datalogWrite += "/";
  datalogWrite += getMonth;
  datalogWrite += "/";
  datalogWrite += getYear;
  datalogWrite += "@";
  datalogWrite += getHour;
  datalogWrite += ":";
  datalogWrite += getMin;
  datalogWrite += ",";
  datalogWrite += CH1_CAL;
  datalogWrite += ",";
  datalogWrite += CH2_CAL;
  
  LFile dataFile = Drv.open("dataloggerReadings.csv", FILE_WRITE); //Open File
  if (dataFile)
  {
    dataFile.println(datalogWrite);
    dataFile.close();

    mainScreen.clear();
    mainScreen.println("Success!");
    mainScreen.println("Log Written");

    //lastMinute
    lastMinute = getMin;

    delay(interval);
    
  }
  else
  {
    mainScreen.clear();
    operationStatus = 2;
    setTitle();
    mainScreen.println("ERROR!");
    mainScreen.println("Failed to Log!");
    mainScreen.println("");
    mainScreen.println("Select to continue...");
    int buttonPressed = digitalRead(button);
    while (button == HIGH)
    { //Do nothing until button is pressed to acknowledge
    }
    mainScreen.clear();
  }
}
