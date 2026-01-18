#include "gnss.h"
#include <time.h>

GnssModule::GnssModule(HardwareSerial &serial) 
  : _serial(serial), _recoveryBufferTimeAnchor(0) {
}

bool GnssModule::begin() {
  if (!_gnss.begin(_serial)) {
    return false;
  }
  
  _gnss.factoryDefault();
  delay(5000);

  _gnss.setUART1Output(COM_TYPE_UBX);
  _gnss.setDynamicModel(DYN_MODEL_PORTABLE);
  _gnss.enableGNSS(true, SFE_UBLOX_GNSS_ID_GPS);
  _gnss.enableGNSS(true, SFE_UBLOX_GNSS_ID_SBAS);
  _gnss.enableGNSS(true, SFE_UBLOX_GNSS_ID_GALILEO);
  _gnss.enableGNSS(true, SFE_UBLOX_GNSS_ID_BEIDOU);
  _gnss.enableGNSS(true, SFE_UBLOX_GNSS_ID_GLONASS);
  _gnss.setHighPrecisionMode(true);
  _gnss.setNavigationFrequency(2);

  return true;
}

void GnssModule::update() {
  _gnss.checkUblox();
}

void GnssModule::getData(GNSS_DATA &data) {
  data.siv = _gnss.getSIV(1);
  data.latRaw = _gnss.getLatitude(0);
  data.lngRaw = _gnss.getLongitude(0);
  data.lat = (double)data.latRaw * 0.0000001;
  data.lng = (double)data.lngRaw * 0.0000001;

  data.alt = (double)_gnss.getAltitude(0) * 0.001;

  data.dateValid = _gnss.getDateValid(0);
  data.timeValid = _gnss.getTimeValid(0);

  time_t unixTimeUTCByGnss = (time_t)_gnss.getUnixEpoch(0);
  _setJstTimeFromUTCUnixTime(unixTimeUTCByGnss, data);
  data.millisecond = _gnss.getMillisecond(0);

  data.fixType = _gnss.getFixType(0);
  data.hdop = _gnss.getHorizontalDOP(0) * 0.01f;
  data.pdop = _gnss.getPositionDOP(0) * 0.01f;
  data.vel = _gnss.getGroundSpeed(0) * 0.0036f; // mm/s -> km/h

  data.isFixOk = _gnss.getGnssFixOk(0);
}

bool GnssModule::isValid(const GNSS_DATA &data) {
  // TODO: チャタリング防止、不安定位置防止メソッドを追加

  if (data.hdop > GNSS_HDOP_THRESHOLD) {
    _recoveryBufferTimeAnchor = millis();
    return false;
  }
  if (data.siv < GNSS_MIN_SATELLITES) {
    _recoveryBufferTimeAnchor = millis();
    return false;
  }
  if (data.isFixOk != true) {
    return false;
  }

  if (fabs(data.lat) < GNSS_POSITION_THRESHOLD) {
    return false;
  }
  if (fabs(data.lng) < GNSS_POSITION_THRESHOLD) {
    return false;
  }

  // 回復時の位置情報は不安定
  if (millis() - _recoveryBufferTimeAnchor < GNSS_RECOVERY_BUFFER_MS) {
    return false;
  }

  return true;
}

void GnssModule::_setJstTimeFromUTCUnixTime(time_t utcTime, GNSS_DATA &data) {
  // utcTime += 9 * 3600; // JST
  utcTime += 8 * 3600;  // 一時的に台湾時間

  struct tm *jstTime = gmtime(&utcTime);
  data.year = jstTime->tm_year + 1900;
  data.month = jstTime->tm_mon + 1;
  data.day = jstTime->tm_mday;
  data.hour = jstTime->tm_hour;
  data.minute = jstTime->tm_min;
  data.second = jstTime->tm_sec;
}
