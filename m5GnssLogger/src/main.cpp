#include <Arduino.h>
#include <M5Core2.h>
#include <AXP192.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

typedef struct
{
  uint8_t siv; // num of satellite
  double lat;
  double lng;
  double alt; // m
  float vel;  // km/s

  bool dateValid;
  uint16_t year;
  uint8_t month;
  uint8_t day;

  bool timeValid;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;

  uint8_t fixType; // 0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix
  uint8_t rtk;     // 0=No solution, 1=Float solution, 2=Fixed solution
  uint32_t accuracy;
  float hdop;

} GNSS_DATA;

// global
SFE_UBLOX_GNSS myGNSS;
AXP192 axp192;

File file;
char fileName[64];

GNSS_DATA gnssData{
  .siv = 0,
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
  .rtk = 0,
  .accuracy = 0,
  .hdop = 99.0f,
};

// function prototype
static bool isGpsValid();
static void gnssDataWriteToSd();
static bool isSDCardReady();
static void getGnssData();

// main
void setup()
{
  M5.begin(true, true, true, true);
  M5.Lcd.setTextFont(2);
  Serial2.begin(38400, SERIAL_8N1, 13, 14); // for NEO_M9N

  if (!myGNSS.begin(Serial2))
  {
    M5.Lcd.print("u-blox GNSS module not detected");
    delay(5000);
    ESP.restart();
  }
  myGNSS.setUART1Output(COM_TYPE_UBX); // ubx may high accuracy...?
  uint8_t initialTimeSecond = 0;

  // waiting for signal be stable
  do
  {
    myGNSS.checkUblox();
    M5.Lcd.print("Waiting Time Signal...");
    getGnssData();
  } while (!(gnssData.timeValid && gnssData.dateValid && (gnssData.second != 0) && (gnssData.day != 0))); // value check

  M5.Lcd.clear(0x000000);
  M5.Lcd.setCursor(0, 0);

  sprintf(fileName, "/gnss_csv_data_%04d%02d%02d_%02d%02d%02d.csv", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);

  // initial write (header)
  if (!isSDCardReady())
  {
    M5.Lcd.setTextColor(M5.Lcd.color565(255, 0, 0));
    M5.Lcd.printf("Error : SDCardNotFound");
    delay(10000);
    ESP.restart();
  }
  char lineStr[64];
  sprintf(lineStr, "date,time,lat,lng,alt,spd,val,hdop");
  file = SD.open(fileName, FILE_APPEND);
  file.println(lineStr);
  file.close();
}

void loop()
{
  static uint8_t preSecond = 0;

  // 0.1msec process
  if (!(millis() % 100))
  {
    M5.update();
    myGNSS.checkUblox(); // これここでいいのか…？

    float batVoltage = axp192.GetBatVoltage();
    bool isGpsOk = isGpsValid();
    bool isSdCardOk = isSDCardReady();

    getGnssData(); // get data and set to gnssData.

    // Date update -> write and display
    if (gnssData.second != preSecond)
    {
      preSecond = gnssData.second;

      gnssDataWriteToSd();

      M5.Lcd.fillScreen(BLACK);   // 画面をクリア
      M5.Lcd.setTextColor(WHITE); // テキストカラーを白に設定
      M5.Lcd.setTextSize(1);      // テキストサイズを設定
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

      // display
      M5.Lcd.printf("Satellites: %d\n", gnssData.siv);
      M5.Lcd.printf("hdop: %.2f\n", gnssData.hdop);
      M5.Lcd.printf("fixType: %d\n", gnssData.fixType);
      M5.Lcd.printf("rtk: %d\n", gnssData.rtk);
      M5.Lcd.printf("accuracy: %d\n", gnssData.accuracy);

      M5.Lcd.printf("Lat: %.6f\n", gnssData.lat);
      M5.Lcd.printf("Lon: %.6f\n", gnssData.lng);
      M5.Lcd.printf("Alt: %.6f\n", gnssData.alt);
      M5.Lcd.printf("Vel: %.1f\n", gnssData.vel);

      M5.Lcd.printf("DT: %04d/%02d/%02d_%02d%02d%02d\n", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
      M5.Lcd.printf("BAT: %.2f\n", batVoltage);
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
  gnssData.lat = (double)myGNSS.getLatitude(0) * 0.0000001;
  gnssData.lng = (double)myGNSS.getLongitude(0) * 0.0000001;
  gnssData.alt = (double)myGNSS.getAltitude(0) * 0.001;

  gnssData.dateValid = myGNSS.getDateValid(0);
  gnssData.year = myGNSS.getYear(0);
  gnssData.month = myGNSS.getMonth(0);
  gnssData.day = myGNSS.getDay(0);

  gnssData.timeValid = myGNSS.getTimeValid(0);
  gnssData.hour = myGNSS.getHour(0);
  gnssData.minute = myGNSS.getMinute(0);
  gnssData.second = myGNSS.getSecond(0);

  gnssData.fixType = myGNSS.getFixType(0);
  gnssData.rtk = myGNSS.getCarrierSolutionType(0);
  gnssData.accuracy = myGNSS.getPositionAccuracy(0);
  gnssData.hdop = myGNSS.getHorizontalDOP(0) * 0.01f;
  gnssData.vel = myGNSS.getGroundSpeed(0) * 0.0036f; // mm/s -> km/h
}

/**
 * @brief data write to SD card
 *
 */
static void gnssDataWriteToSd()
{

  if (!isGpsValid() || !isSDCardReady())
  {
    return;
  }

  char datetimeUtc[32]; // TODO: UTC->JST convert
  sprintf(datetimeUtc, "%04d/%02d/%02d,%02d:%02d:%02d", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
  char lineStr[128];
  sprintf(lineStr, "%s,%lf,%lf,%.1f,%.0f,%d,%2f", datetimeUtc, gnssData.lat, gnssData.lng, gnssData.alt, gnssData.vel, gnssData.siv, gnssData.hdop);

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
  if (gnssData.hdop > 2.5f)
  { // tmp
    recover_buffer_time_anchor = millis();
    return false;
  }
  if (gnssData.siv < 10)
  { // tmp
    recover_buffer_time_anchor = millis();
    return false;
  }
  if (fabs(gnssData.lat) < 0.001f)
  {
    recover_buffer_time_anchor = millis();
    return false;
  }
  if (fabs(gnssData.lng) < 0.001f)
  {
    recover_buffer_time_anchor = millis();
    return false;
  }

  // position info is unstable when recover
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