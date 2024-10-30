#include <Arduino.h>
#include <TinyGPSPlus.h>  //for M5 GNSS module
#include <M5Core2.h>

TinyGPSPlus gps;  

void setup() {
  M5.begin(true,true,true,true);
  M5.Lcd.setTextFont(2);
  Serial.begin(115200);
  Serial2.begin(38400, SERIAL_8N1, 13, 14); //for GPS

}

void loop() {
  while(Serial2.available()>0){
    if(gps.encode(Serial2.read())){ //get data
      break;
    }
  }

  //test print
  M5.Lcd.printf("val:%d\r\n, valid:%1d\r\n",gps.satellites.value(),gps.satellites.isValid());
  M5.Lcd.printf("hdop:%f\r\n, valid:%1d\r\n",gps.hdop.hdop(),gps.hdop.isValid());
  M5.Lcd.printf("lat:%f\r\n, valid:%1d\r\n",gps.location.lat(),gps.location.isValid());
  M5.Lcd.printf("lng:%f\r\n, valid:%1d\r\n",gps.location.lng(),gps.location.isValid());
  M5.Lcd.printf("date:%02d%02d%02d\r\n, valid:%1d\r\n",gps.date.year(),gps.date.month(),gps.date.day(),gps.location.isValid());
  M5.Lcd.printf("time:%02d%02d%02d\r\n, valid:%1d\r\n",gps.time.hour(),gps.time.minute(),gps.time.second(),gps.location.isValid());


  delay(1000);
  M5.Lcd.clear(0x000000);
  M5.Lcd.setCursor(0,0);
}

