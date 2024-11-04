// refelence: https://github.com/m5stack/M5Module-GNSS

#include <Arduino.h>
#include <TinyGPSPlus.h> //for M5 GNSS module
#include <M5Core2.h>
#include <AXP192.h>

TinyGPSPlus gps;
bool gpsValid = false;

File file;
char fileName[32];

AXP192 axp192;

static void smartDelay(unsigned long);
static bool isGpsReady(TinyGPSPlus);
static void gpsDataWriteToSd(TinyGPSPlus);
static bool isSDCardReady();

void setup()
{
  M5.begin(true, true, true, true);
  M5.Lcd.setTextFont(2);
  // Serial.begin(115200);
  Serial2.begin(38400, SERIAL_8N1, 13, 14); // for GPS

  bool gpsReady = false;
  while (!gpsReady)
  {
    M5.Lcd.clear(0x000000);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("Waiting for GNSS ready.....");
    M5.Lcd.printf("val:%d\r\n, valid:%1d\r\n", gps.satellites.value(), gps.satellites.isValid());

    if (gps.satellites.isValid() && gps.date.isValid())
      gpsReady = true;
    smartDelay(1000);
  }
  M5.Lcd.clear(0x000000);
  M5.Lcd.setCursor(0, 0);

  sprintf(fileName, "/gpx_data_%02d%02d%02d%02d%02d%02d.csv", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second());

  // initial write

  if (!isSDCardReady())
  {
    M5.Lcd.setTextColor(M5.Lcd.color565(255, 0, 0));
    M5.Lcd.printf("Error : SDCardNotFound");
    delay(10000);
  }
  char lineStr[128];
  sprintf(lineStr, "date,time,lat,lng,alt");
  file = SD.open(fileName, FILE_APPEND);
  file.println(lineStr);
  file.close();
}

void loop()
{
  M5.update();
  bool isGpsOk = isGpsReady(gps);
  bool isSdCardOk = isSDCardReady();
  float batVoltage = axp192.GetBatVoltage();

  // test print
  if (isGpsOk || isSdCardOk || batVoltage > 3.6f)
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

  M5.Lcd.printf("val:%d\r\n, valid:%1d\r\n", gps.satellites.value(), gps.satellites.isValid());
  M5.Lcd.printf("hdop:%f\r\n, valid:%1d\r\n", gps.hdop.hdop(), gps.hdop.isValid());
  M5.Lcd.printf("lat:%f\r\n, valid:%1d\r\n", gps.location.lat(), gps.location.isValid());
  M5.Lcd.printf("lng:%f\r\n, valid:%1d\r\n", gps.location.lng(), gps.location.isValid());
  M5.Lcd.printf("alt:%f\r\n, valid:%1d\r\n", gps.altitude.meters(), gps.altitude.isValid());
  M5.Lcd.printf("date:%02d%02d%02d_%02d%02d%02d\r\n, valid:%1d\r\n", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.isValid());
  M5.Lcd.printf("BATT: %4f\r\n", batVoltage);

  gpsDataWriteToSd(gps);

  smartDelay(1000);
  M5.Lcd.clear(0x000000);
  M5.Lcd.setCursor(0, 0);
}
// !main


static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (Serial2.available())
      gps.encode(Serial2.read());
  } while (millis() - start < ms);
}

static void gpsDataWriteToSd(TinyGPSPlus _gps)
{

  if (!isGpsReady(_gps))
  {
    return;
  }

  // cardcheck

  char datetimeUtc[20];
  sprintf(datetimeUtc, "%02d/%02d/%02d,%02d:%02d:%02d", _gps.date.year(), _gps.date.month(), _gps.date.day(), _gps.time.hour(), _gps.time.minute(), _gps.time.second());
  char lineStr[128];
  sprintf(lineStr, "%s,%f,%f,%.1f", datetimeUtc, _gps.location.lat(), _gps.location.lng(), _gps.altitude.meters());

  file = SD.open(fileName, FILE_APPEND);
  file.println(lineStr);
  file.close();
}

//TODO add anti-chattering 
static bool isGpsReady(TinyGPSPlus _gps)
{
  if (!_gps.hdop.isValid())
    return false;
  if (_gps.hdop.hdop() > 50)
    return false;

  if (fabs(_gps.location.lat()) < 1.0f)
    return false;

  if (fabs(_gps.location.lng()) < 1.0f)
    return false;

  return true;
}

static bool isSDCardReady()
{
  sdcard_type_t Type = SD.cardType();
  if (Type == CARD_UNKNOWN || Type == CARD_NONE)
  {
    return false;
  }
  return true;
}