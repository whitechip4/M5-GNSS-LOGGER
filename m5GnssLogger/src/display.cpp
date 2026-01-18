#include "display.h"

DisplayModule::DisplayModule() {
}

void DisplayModule::begin() {
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(1);
}

void DisplayModule::clear() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
}

void DisplayModule::showMessage(const char *message) {
  M5.Lcd.print(message);
}

void DisplayModule::update(const GNSS_DATA &data, float batteryVoltage, 
                          bool isGpsOk, bool isSdCardOk, DISPLAY_MODE mode) {
  clear();
  M5.Lcd.setTextColor(WHITE);

  if (isGpsOk && isSdCardOk && batteryVoltage > BATTERY_VOLTAGE_THRESHOLD) {
    M5.Lcd.printf("Status : ");
    M5.Lcd.setTextColor(M5.Lcd.color565(0, 255, 0));
    M5.Lcd.printf("Recording\r\n");
  } else {
    M5.Lcd.printf("Status : ");
    M5.Lcd.setTextColor(M5.Lcd.color565(255, 0, 0));
    M5.Lcd.printf("Stop\r\n");
  }
  M5.Lcd.setTextColor(M5.Lcd.color565(255, 255, 255));

  if (mode == DISPLAY_MODE_DETAIL) {
    _showDetailMode(data, batteryVoltage, isGpsOk, isSdCardOk);
  } else {
    _showSimpleMode(data);
  }
}

void DisplayModule::_showDetailMode(const GNSS_DATA &data, float batteryVoltage, 
                                   bool isGpsOk, bool isSdCardOk) {
  M5.Lcd.printf("Satellites: %d\n", data.siv);
  M5.Lcd.printf("hdop: %.2f\n", data.hdop);
  M5.Lcd.printf("pdop: %.2f\n", data.pdop);

  M5.Lcd.printf("isFixOK: %d\n", data.isFixOk);
  M5.Lcd.printf("fixtype: %d\n", data.fixType);

  M5.Lcd.printf("Lat: %.6f\n", data.lat);
  M5.Lcd.printf("Lon: %.6f\n", data.lng);

  M5.Lcd.printf("Alt: %.6f\n", data.alt);
  M5.Lcd.printf("Vel: %.1f\n", data.vel);

  M5.Lcd.printf("DT: %04d/%02d/%02d_%02d%02d%02d\n", 
                data.year, data.month, data.day, 
                data.hour, data.minute, data.second);
  M5.Lcd.printf("BAT: %.2f\n", batteryVoltage);
}

void DisplayModule::_showSimpleMode(const GNSS_DATA &data) {
  M5.Lcd.setTextSize(5);
  M5.Lcd.printf("%5.0f", data.vel);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf(" km/h");
  M5.Lcd.setTextSize(5);
  M5.Lcd.printf("\n");
  
  M5.Lcd.setTextSize(1);
  M5.Lcd.printf("-----------------\n");
  
  M5.Lcd.setTextSize(3);
  M5.Lcd.printf("%7.0f", data.alt);
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("   m high\n");
  M5.Lcd.setTextSize(3);
  M5.Lcd.printf("\n");
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.printf("Time: %02d:%02d:%02d\n", 
                data.hour, data.minute, data.second);
  
  // テキストサイズをデフォルトに戻す
  M5.Lcd.setTextSize(1);
}
