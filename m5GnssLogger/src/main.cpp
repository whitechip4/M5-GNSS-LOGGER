#include <Arduino.h>
#include <M5Core2.h>
#include <AXP192.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

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
  uint8_t month;
  uint8_t day;

  bool timeValid;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;

  uint8_t fixType; // 0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix
  float hdop;
  float pdop;

  // for karman filter
  float latEstimate;         // 推定された緯度
  float lngEstimate;         // 推定された経度
  float latCovariance;       // 緯度の共分散
  float lngCovariance;       // 経度の共分散
  float posProcessNoise;     // プロセスノイズ
  float posMeasurementNoise; // 測定ノイズ

  // for spd karman filter
  float velEstimate;
  float velCovariance;
  float velProcessNoise;
  float velMeasurementNoise;

} GNSS_DATA;

// global
SFE_UBLOX_GNSS myGNSS;
AXP192 axp192;

File file;
char fileName[64];
char fileNameKarman[128];

float dt = 1.0; // GNSS sampling time
uint32_t preCalculateTick = 0;
uint32_t calculateTick = 0;

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

    // for karmanfilter
    .latEstimate = 0.0f,   // 推定された緯度
    .lngEstimate = 0.0f,   // 推定された経度
    .latCovariance = 1.0f, // 緯度の共分散
    .lngCovariance = 1.0f, // 経度の共分散

    .posProcessNoise = 0.1f,     // プロセスノイズ
    .posMeasurementNoise = 1.0f, // 測定ノイズ

    .velEstimate = 0.0f,
    .velCovariance = 1.0f,
    .velProcessNoise = 0.1f,
    .velMeasurementNoise = 1.0f,

};
float batVoltage = 0;
bool isGpsOk = false;
bool isSdCardOk = false;

// function prototype
static bool isGpsValid();
static void gnssDataWriteToSd();
static bool isSDCardReady();
static void getGnssData();
static void updateDisplay();
static void karmanFilter();

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
  myGNSS.setHighPrecisionMode(true);

  uint8_t initialTimeSecond = 0;

  // waiting for get time signal
  do
  {
    M5.Lcd.print("Waiting Time Signal...\n");
    M5.Lcd.printf("DT: %04d/%02d/%02d_%02d%02d%02d\n", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
    myGNSS.checkUblox();
    getGnssData();
    delay(1000);
    M5.Lcd.clear(0x000000);
    M5.Lcd.setCursor(0, 0);
  } while (!(gnssData.timeValid && gnssData.dateValid && (gnssData.second != 0) && (gnssData.day != 0))); // value check

  sprintf(fileName, "/gnss_csv_data_%04d%02d%02d_%02d%02d%02d.csv", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
  sprintf(fileNameKarman, "/gnss_csv_data_%04d%02d%02d_%02d%02d%02d_karman.csv", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);

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
  file = SD.open(fileNameKarman, FILE_APPEND);
  file.println(lineStr);
  file.close();

  do
  {
    M5.Lcd.print("Waiting gnss data be stable...\n");
    myGNSS.checkUblox();
    getGnssData();
    delay(500);
    M5.Lcd.clear(0x000000);
    M5.Lcd.setCursor(0, 0);

  } while ((!isGpsValid()) && (gnssData.siv > 10));
  // initialize estimate pos
  gnssData.latEstimate = gnssData.lat;
  gnssData.lngEstimate = gnssData.lng;
  gnssData.velEstimate = gnssData.vel;
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

      calculateTick = millis();
      dt = (calculateTick - preCalculateTick) * 0.001;

      preSecond = gnssData.second;

      gnssDataWriteToSd();
      karmanFilter();
      updateDisplay();
      preCalculateTick = calculateTick;
    }
  }
}
// !main

static void karmanFilter()
{
  // for pos
  float kSatteliteNoize = constrain(2.0 / gnssData.siv, 0.01, 2.0) * 10.0f;
  gnssData.posMeasurementNoise = (gnssData.hdop * 2.0f) * kSatteliteNoize;
  gnssData.posProcessNoise = constrain(gnssData.vel * 0.1f ,0.01f,10.0f) * kSatteliteNoize;

  // predict
  gnssData.latCovariance += gnssData.posProcessNoise;
  gnssData.lngCovariance += gnssData.posProcessNoise;

  // karman gain
  double kalmanGainLat = gnssData.latCovariance / (gnssData.latCovariance + gnssData.posMeasurementNoise);
  double kalmanGainLng = gnssData.lngCovariance / (gnssData.latCovariance + gnssData.posMeasurementNoise);

  // estimate
  gnssData.latEstimate += kalmanGainLat * (gnssData.lat - gnssData.latEstimate);
  gnssData.lngEstimate += kalmanGainLng * (gnssData.lng - gnssData.lngEstimate);

  // update
  gnssData.latCovariance *= (1 - kalmanGainLat);
  gnssData.lngCovariance *= (1 - kalmanGainLng);


  // ----- for vel
  gnssData.posMeasurementNoise = (gnssData.pdop * 1.0f) * kSatteliteNoize;
  gnssData.posProcessNoise = (gnssData.pdop * 0.1f) * kSatteliteNoize;

  // predict
  gnssData.velCovariance += gnssData.velProcessNoise;

  // karman gain
  double kalmanGainVel = gnssData.velCovariance / (gnssData.velCovariance + gnssData.velMeasurementNoise);

  // estimate
  gnssData.velEstimate += kalmanGainVel * (gnssData.vel - gnssData.velEstimate);

  // update
  gnssData.velCovariance *= (1 - kalmanGainVel);
}

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
  gnssData.year = myGNSS.getYear(0);
  gnssData.month = myGNSS.getMonth(0);
  gnssData.day = myGNSS.getDay(0);

  gnssData.timeValid = myGNSS.getTimeValid(0);
  gnssData.hour = myGNSS.getHour(0);
  gnssData.minute = myGNSS.getMinute(0);
  gnssData.second = myGNSS.getSecond(0);

  gnssData.fixType = myGNSS.getFixType(0);
  gnssData.hdop = myGNSS.getHorizontalDOP(0) * 0.01f;
  gnssData.pdop = myGNSS.getPositionDOP(0) * 0.01f;
  gnssData.vel = myGNSS.getGroundSpeed(0) * 0.0036f; // mm/s -> km/h
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

  // display
  M5.Lcd.printf("Satellites: %d\n", gnssData.siv);
  M5.Lcd.printf("hdop: %.2f\n", gnssData.hdop);
  M5.Lcd.printf("pdop: %.2f\n", gnssData.pdop);

  // M5.Lcd.printf("dt: %f\n", dt);

  M5.Lcd.printf("Lat: %.6f\n", gnssData.lat);
  M5.Lcd.printf("Lon: %.6f\n", gnssData.lng);

  M5.Lcd.printf("LatKar: %.6f\n", gnssData.latEstimate);
  M5.Lcd.printf("LonKar: %.6f\n", gnssData.lngEstimate);
  M5.Lcd.printf("mNz: %.6f\n", gnssData.posMeasurementNoise);
  M5.Lcd.printf("pNz: %.6f\n", gnssData.posProcessNoise);

  // M5.Lcd.printf("Alt: %.6f\n", gnssData.alt);
  M5.Lcd.printf("Vel: %.1f\n", gnssData.vel);
  M5.Lcd.printf("VelEst: %.1f\n", gnssData.velEstimate);

  // M5.Lcd.printf("DT: %04d/%02d/%02d_%02d%02d%02d\n", gnssData.year, gnssData.month, gnssData.day, gnssData.hour, gnssData.minute, gnssData.second);
  M5.Lcd.printf("BAT: %.2f\n", batVoltage);
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
  sprintf(lineStr, "%s,%lf,%lf,%.1f,%.1f,%d,%2f", datetimeUtc, gnssData.lat, gnssData.lng, gnssData.alt, gnssData.vel, gnssData.siv, gnssData.hdop);

  file = SD.open(fileName, FILE_APPEND);
  file.println(lineStr);
  file.close();

  sprintf(lineStr, "%s,%lf,%lf,%.1f,%.1f,%d,%2f", datetimeUtc, gnssData.latEstimate, gnssData.lngEstimate, gnssData.alt, gnssData.vel, gnssData.siv, gnssData.hdop);

  file = SD.open(fileNameKarman, FILE_APPEND);
  file.println(lineStr);
  file.close();
}

/**
 * @brief check GPS status and judge valid or not
 *
 */
static bool isGpsValid()
{
  // static uint32_t recover_buffer_time_anchor = millis();

  // TODO add anti-chattering, anti_unstable_position method
  // if (gnssData.hdop > 2.0f)
  // { // tmp
  //   recover_buffer_time_anchor = millis();
  //   return false;
  // }
  // if (gnssData.siv < 11)
  // { // tmp
  //   recover_buffer_time_anchor = millis();
  //   return false;
  // }

  if (gnssData.hdop > 90.f)
  { // tmp
    return false;
  }
  if (gnssData.siv < 1)
  { // tmp
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
  // if (millis() - recover_buffer_time_anchor < 5000)
  // {
  //   return false;
  // }

  // //speed limit remove drift
  // if( fabs(gnssData.vel ) < 1.5f){
  //   return false;
  // }

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