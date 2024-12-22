#include <Arduino.h>
#include <M5Core2.h>
#include <AXP192.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include <time.h>

typedef struct
{
  uint8_t siv; // num of satellite
  int32_t latRaw;
  int32_t lngRaw;
  double lat; // double is 32bit on m5stack core2....
  double lng;

  double alt; // m
  float vel;  // km/s

  bool dateValid;
  uint16_t year;
  uint16_t month;
  uint16_t day;

  bool timeValid;
  uint16_t hour;
  uint16_t minute;
  uint16_t second;
  uint16_t millisecond;

  uint8_t fixType; // 0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix
  float hdop;
  float pdop;

  bool isFixOk;

} GNSS_DATA;

// global
SFE_UBLOX_GNSS myGNSS;
AXP192 axp192;

File file;
char fileName[64];
char fileRawDataName[128];

GNSS_DATA gnssData{
    .siv = 0,
    .latRaw = 0,
    .lngRaw = 0,
    .lat = 0.0,
    .lng = 0.0,

    .alt = 0.0,
    .vel = 0.0f,

    .dateValid = false,
    .year = 0,
    .month = 0,
    .day = 0,

    .timeValid = false,
    .hour = 0,
    .minute = 0,
    .second = 0,

    .fixType = 0,
    .hdop = 99.0f,
    .pdop = 99.0f,

    .isFixOk = false,

};
float batVoltage = 0;
bool isGpsOk = false;
bool isSdCardOk = false;

uint8_t viewMode = 0;

uint32_t vibEndTimeMillis = 0;
bool vibFlag = false;


// function prototype
static bool isGpsValid();
static void writeGnssDataToSd();
static bool isSDCardReady();
static void getGnssData();
static void updateDisplay();
static void setJstTimeFromUTCUnixTime(time_t utcTime, GNSS_DATA &_gnssData);

static void viblation(uint32_t ms);
static void viblationProcess();

// main
void setup()
{
  M5.begin(true, true, true, true);
  M5.Lcd.setTextFont(2);
  Serial2.begin(38400, SERIAL_8N1, 13, 14); // for NEO_M9N
  M5.Lcd.print("Initializing...\n");

  if (!myGNSS.begin(Serial2))
  {
    M5.Lcd.print("u-blox GNSS module not detected");
    delay(5000);
    ESP.restart();
  }
  myGNSS.factoryDefault();
  delay(5000);

  myGNSS.setUART1Output(COM_TYPE_UBX); // ubx may high accuracy...?
  myGNSS.setDynamicModel(DYN_MODEL_PORTABLE);
  myGNSS.enableGNSS(true, SFE_UBLOX_GNSS_ID_GPS); // Make sure GPS is enabled (we must leave at least one major GNSS enabled!)
  myGNSS.enableGNSS(true, SFE_UBLOX_GNSS_ID_SBAS); // 
  myGNSS.enableGNSS(true, SFE_UBLOX_GNSS_ID_GALILEO); // 
  myGNSS.enableGNSS(true, SFE_UBLOX_GNSS_ID_BEIDOU); //
  myGNSS.enableGNSS(true, SFE_UBLOX_GNSS_ID_GLONASS); // 
  myGNSS.setHighPrecisionMode(true);
  myGNSS.setNavigationFrequency(2);
  M5.Lcd.clear(0x000000);
  M5.Lcd.setCursor(0, 0);

  uint8_t initialTimeSecond = 0;

  // waiting for get time signal
  do
  {
    M5.Lcd.print("Waiting for receive Time Signal...\n");
    M5.Lcd.printf("DT: %04d/%02d/%02d_%02d%02d%02d\n", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
    myGNSS.checkUblox();
    getGnssData();
    delay(1000);
    M5.Lcd.clear(0x000000);
    M5.Lcd.setCursor(0, 0);
  } while (!(gnssData.timeValid && gnssData.dateValid && (gnssData.second != 0) && (gnssData.day != 0))); // value check

  sprintf(fileName, "/gnss_csv_data_%04d%02d%02d_%02d%02d%02d.csv", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
  sprintf(fileRawDataName, "/gnss_csv_data_%04d%02d%02d_%02d%02d%02d_raw.csv", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);

  // initial write (header)
  if (!isSDCardReady())
  {
    M5.Lcd.setTextColor(M5.Lcd.color565(255, 0, 0));
    M5.Lcd.printf("Error : SDCardNotFound");
    delay(10000);
    ESP.restart();
  }
  char lineStr[64];
  sprintf(lineStr, "date,time,lat,lng,alt,spd,siv,hdop");
  file = SD.open(fileName, FILE_APPEND);
  file.println(lineStr);
  file.close();

  sprintf(lineStr, "date,time,lat,lng,alt,spd,siv,hdop");
  file = SD.open(fileRawDataName, FILE_APPEND);
  file.println(lineStr);
  file.close();

  do
  {
    M5.Lcd.print("Waiting gnss data be stable...\n");
    M5.Lcd.printf("Satellites: %d (>= 7)\n", gnssData.siv);
    myGNSS.checkUblox();
    getGnssData();
    delay(500);
    M5.Lcd.clear(0x000000);
    M5.Lcd.setCursor(0, 0);

  } while ((!isGpsValid()) && (gnssData.siv < 7));
}

void loop()
{
  static uint8_t preSecond = 0;

  // 0.1msec process
  if (!(millis() % 100))
  {
    // update
    M5.update();
    myGNSS.checkUblox(); // read data from serial.
    getGnssData();       // get data and set to gnssData.
    batVoltage = axp192.GetBatVoltage();
    isGpsOk = isGpsValid();
    isSdCardOk = isSDCardReady();

    // Date update -> write and display
    if (gnssData.second != preSecond)
    {
      preSecond = gnssData.second;

      writeGnssDataToSd();
      updateDisplay();

    }

    if(M5.BtnA.wasPressed()){
      viewMode = 1 - viewMode;
      viblation(200);
    }

    //1msec process
    if(!(millis() % 1)){
      viblationProcess();
    }

  }
}
// !main

/**
 * @brief Get the Gnss Data from instance and set to gnss data object
 *
 */
static void getGnssData()
{

  gnssData.siv = myGNSS.getSIV(1);
  gnssData.latRaw = myGNSS.getLatitude(0);
  gnssData.lngRaw = myGNSS.getLongitude(0);
  gnssData.lat = (double)gnssData.latRaw * 0.0000001;
  gnssData.lng = (double)gnssData.lngRaw * 0.0000001;

  gnssData.alt = (double)myGNSS.getAltitude(0) * 0.001;

  gnssData.dateValid = myGNSS.getDateValid(0);
  gnssData.timeValid = myGNSS.getTimeValid(0);

  time_t unixTimeUTCByGnss = (time_t)myGNSS.getUnixEpoch(0);  // unixEpoch is UTC and seconds order
  setJstTimeFromUTCUnixTime(unixTimeUTCByGnss, gnssData);
  gnssData.millisecond = myGNSS.getMillisecond(0);

  gnssData.fixType = myGNSS.getFixType(0);
  gnssData.hdop = myGNSS.getHorizontalDOP(0) * 0.01f;
  gnssData.pdop = myGNSS.getPositionDOP(0) * 0.01f;
  gnssData.vel = myGNSS.getGroundSpeed(0) * 0.0036f; // mm/s -> km/h

  gnssData.isFixOk = myGNSS.getGnssFixOk(0);
}

/** 
 * @brief set JST time for gnssData from UTC unix time
 * 
 */

static void setJstTimeFromUTCUnixTime(time_t utcTime, GNSS_DATA &_gnssData)
{
  // utcTime += 9 * 3600;
  utcTime += 8 * 3600;  //temp Taiwan

  struct tm *jstTime = gmtime(&utcTime);
  _gnssData.year = jstTime->tm_year + 1900;
  _gnssData.month = jstTime->tm_mon + 1;
  _gnssData.day = jstTime->tm_mday;
  _gnssData.hour = jstTime->tm_hour;
  _gnssData.minute = jstTime->tm_min;
  _gnssData.second = jstTime->tm_sec;
}

/**
 * @brief update M5 LCD
 *
 */
static void updateDisplay()
{
  // tempolary
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 0);

  if (isGpsOk && isSdCardOk && batVoltage > 3.6f)
  {
    M5.Lcd.printf("Status : ");
    M5.Lcd.setTextColor(M5.Lcd.color565(0, 255, 0));
    M5.Lcd.printf("Recording\r\n");
  }
  else
  {
    M5.Lcd.printf("Status : ");
    M5.Lcd.setTextColor(M5.Lcd.color565(255, 0, 0));
    M5.Lcd.printf("Stop\r\n");
  }
  M5.Lcd.setTextColor(M5.Lcd.color565(255, 255, 255));

  if(viewMode == 0){

    // display
    M5.Lcd.printf("Satellites: %d\n", gnssData.siv);
    M5.Lcd.printf("hdop: %.2f\n", gnssData.hdop);
    M5.Lcd.printf("pdop: %.2f\n", gnssData.pdop);

    M5.Lcd.printf("isFixOK: %d\n",gnssData.isFixOk);
    M5.Lcd.printf("fixtype: %d\n",gnssData.fixType);

    M5.Lcd.printf("Lat: %.6f\n", gnssData.lat);
    M5.Lcd.printf("Lon: %.6f\n", gnssData.lng);

    M5.Lcd.printf("Alt: %.6f\n", gnssData.alt);
    M5.Lcd.printf("Vel: %.1f\n", gnssData.vel);

    M5.Lcd.printf("DT: %04d/%02d/%02d_%02d%02d%02d\n", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
    M5.Lcd.printf("BAT: %.2f\n", batVoltage);
  }else{
    //tempolary...
      M5.Lcd.setTextSize(5);
      M5.Lcd.printf("%5.0f", gnssData.vel);M5.Lcd.setTextSize(2);M5.Lcd.printf(" km/h");M5.Lcd.setTextSize(5);M5.Lcd.printf("\n");
      M5.Lcd.setTextSize(1);M5.Lcd.printf("-----------------\n");
      M5.Lcd.setTextSize(3);
      M5.Lcd.printf("%7.0f", gnssData.alt);M5.Lcd.setTextSize(2);M5.Lcd.printf("   m high\n");M5.Lcd.setTextSize(3);M5.Lcd.printf("\n");
      
      M5.Lcd.setTextSize(2);M5.Lcd.printf("Time: %02d:%02d:%02d\n", gnssData.hour, gnssData.minute, gnssData.second);

  }
}

/**
 * @brief data write to SD card
 *
 */
static void writeGnssDataToSd()
{

  if (!isSdCardOk)
  {
    return;
  }


  char datetimeUtc[32]; // TODO: UTC->JST convert
  sprintf(datetimeUtc, "%04d/%02d/%02d,%02d:%02d:%02d", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
  char lineStr[128];
  sprintf(lineStr, "%s,%lf,%lf,%.1f,%.1f,%d,%2f", datetimeUtc, gnssData.lat, gnssData.lng, gnssData.alt, gnssData.vel, gnssData.siv, gnssData.hdop);
  
  // raw data for post process
  file = SD.open(fileRawDataName, FILE_APPEND);
  file.println(lineStr);
  file.close();

  if (!isGpsOk)
  {
    return;
  }

  // only valid data.
  file = SD.open(fileName, FILE_APPEND);
  file.println(lineStr);
  file.close();

}

/**
 * @brief check GPS status and judge valid or not
 *
 */
static bool isGpsValid()
{
  static uint32_t recover_buffer_time_anchor = millis();

  // TODO add anti-chattering, anti_unstable_position method

  if (gnssData.hdop > 6.0f)
  { // tmp
    recover_buffer_time_anchor = millis();
    return false;
  }
  if (gnssData.siv < 5)
  { // tmp
    recover_buffer_time_anchor = millis();
    return false;
  }
  if(gnssData.isFixOk != true){
    return false;
  }

  if (fabs(gnssData.lat) < 0.001f)
  {
    // recover_buffer_time_anchor = millis();
    return false;
  }
  if (fabs(gnssData.lng) < 0.001f)
  {
    // recover_buffer_time_anchor = millis();
    return false;
  }

  // // position info is unstable when recover
  if (millis() - recover_buffer_time_anchor < 5000)
  {
    return false;
  }

  return true;
}

/**
 * @brief check SDCard is empty or not
 *
 */
static bool isSDCardReady()
{
  sdcard_type_t Type = SD.cardType();
  if (Type == CARD_UNKNOWN || Type == CARD_NONE)
  {
    return false;
  }
  return true;
}


static void viblation(uint32_t ms){
  vibEndTimeMillis = millis() + ms;
  vibFlag = true;
  M5.Axp.SetLDOEnable(3, true); // 振動モーターを付ける
}

static void viblationProcess (){

  if(!vibFlag){
    return;
  }

  if(millis() >= vibEndTimeMillis){
    vibFlag = false;
    M5.Axp.SetLDOEnable(3, false); // 振動モーターを消す
  }
}