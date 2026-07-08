#include "gnss.h"
#include "config.h"
#include <time.h>

GnssModule::GnssModule(HardwareSerial& serial)
    : _serial(serial)
    , _recoveryBufferTimeAnchor(0)
    , _hasLastValid(false)
    , _lastValidLat(0.0)
    , _lastValidLng(0.0)
    , _lastValidAlt(0.0)
    , _lastValidMs(0)
    , _hasJumpCandidate(false)
    , _candidateLat(0.0)
    , _candidateLng(0.0)
    , _candidateAlt(0.0)
    , _candidateMs(0)
    , _candidateStartMs(0) {
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

void GnssModule::getData(GNSS_DATA& data) {
  data.siv = _gnss.getSIV(1);
  data.latRaw = _gnss.getLatitude(0);
  data.lngRaw = _gnss.getLongitude(0);
  data.lat = (double)data.latRaw * 0.0000001;
  data.lng = (double)data.lngRaw * 0.0000001;

  data.alt = (double)_gnss.getAltitude(0) * 0.001;

  data.dateValid = _gnss.getDateValid(0);
  data.timeValid = _gnss.getTimeValid(0);

  time_t unixTimeUTCByGnss = (time_t)_gnss.getUnixEpoch(0);
  _setLocalTimeFromUTCUnixTime(unixTimeUTCByGnss, data);
  data.millisecond = _gnss.getMillisecond(0);

  data.fixType = _gnss.getFixType(0);
  data.hdop = _gnss.getHorizontalDOP(0) * 0.01f;
  data.pdop = _gnss.getPositionDOP(0) * 0.01f;
  data.vel = _gnss.getGroundSpeed(0) * 0.0036f;  // mm/s -> km/h

  data.hacc = _gnss.getHorizontalAccEst(0) * 0.001f;  // mm -> m
  data.vacc = _gnss.getVerticalAccEst(0) * 0.001f;    // mm -> m

  data.isFixOk = _gnss.getGnssFixOk(0);
}

bool GnssModule::isValid(const GNSS_DATA& data) {
  if (data.hdop > GNSS_HDOP_THRESHOLD) {
    _recoveryBufferTimeAnchor = millis();
    return false;
  }
  if (data.siv < GNSS_MIN_SATELLITES) {
    _recoveryBufferTimeAnchor = millis();
    return false;
  }
  if (data.isFixOk != true) {
    _recoveryBufferTimeAnchor = millis();
    return false;
  }

  if (fabs(data.lat) < GNSS_POSITION_THRESHOLD) {
    return false;
  }
  if (fabs(data.lng) < GNSS_POSITION_THRESHOLD) {
    return false;
  }

  // 受信機自身の水平精度推定が悪い場合は棄却（マルチパス発散の主要な検出手段）
  if (data.hacc > GNSS_HACC_THRESHOLD_M) {
    _recoveryBufferTimeAnchor = millis();
    return false;
  }

  // 物理的にあり得ない速度は棄却
  if (data.vel > GNSS_MAX_SPEED_KMH) {
    _recoveryBufferTimeAnchor = millis();
    return false;
  }

  // 回復時の位置情報は不安定
  if (millis() - _recoveryBufferTimeAnchor < GNSS_RECOVERY_BUFFER_MS) {
    return false;
  }

  // 最後の有効位置から到達不可能な位置へのジャンプを検出
  uint32_t nowMs = millis();
  if (_hasLastValid &&
      !_isWithinEnvelope(_lastValidLat, _lastValidLng, _lastValidAlt, _lastValidMs, data, nowMs)) {
    // ジャンプ先が一貫し続けた場合のみ、本当に移動したとみなして受け入れる
    // （トンネル明けや屋内からの復帰に対応）
    bool consistentWithCandidate =
        _hasJumpCandidate &&
        _isWithinEnvelope(_candidateLat, _candidateLng, _candidateAlt, _candidateMs, data, nowMs);

    if (!consistentWithCandidate) {
      _candidateStartMs = nowMs;
    }
    _hasJumpCandidate = true;
    _candidateLat = data.lat;
    _candidateLng = data.lng;
    _candidateAlt = data.alt;
    _candidateMs = nowMs;

    if (nowMs - _candidateStartMs < GNSS_JUMP_REACCEPT_MS) {
      return false;
    }
    // 一貫性が確認できたので新しい位置として受け入れる（下で基準位置を更新）
  }

  _hasJumpCandidate = false;
  _hasLastValid = true;
  _lastValidLat = data.lat;
  _lastValidLng = data.lng;
  _lastValidAlt = data.alt;
  _lastValidMs = nowMs;

  return true;
}

double GnssModule::_distanceM(double lat1, double lng1, double lat2, double lng2) {
  const double kEarthRadiusM = 6371000.0;
  double p1 = radians(lat1);
  double p2 = radians(lat2);
  double dp = radians(lat2 - lat1);
  double dl = radians(lng2 - lng1);
  double a = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
  return 2.0 * kEarthRadiusM * asin(sqrt(a));
}

bool GnssModule::_isWithinEnvelope(double fromLat,
                                   double fromLng,
                                   double fromAlt,
                                   uint32_t fromMs,
                                   const GNSS_DATA& data,
                                   uint32_t nowMs) {
  double dtSec = (double)(nowMs - fromMs) * 0.001;
  if (dtSec < 0.5) {
    dtSec = 0.5;
  }
  double maxHorizontalM = (GNSS_MAX_SPEED_KMH / 3.6) * dtSec + GNSS_JUMP_MARGIN_M;
  double maxVerticalM = GNSS_MAX_VERTICAL_SPEED_MPS * dtSec + GNSS_VERTICAL_JUMP_MARGIN_M;

  if (_distanceM(fromLat, fromLng, data.lat, data.lng) > maxHorizontalM) {
    return false;
  }
  if (fabs(data.alt - fromAlt) > maxVerticalM) {
    return false;
  }
  return true;
}

void GnssModule::_setLocalTimeFromUTCUnixTime(time_t utcTime, GNSS_DATA& data) {
  utcTime += AppConfig::getTimezoneOffset() * 3600;

  struct tm* localTime = gmtime(&utcTime);
  data.year = localTime->tm_year + 1900;
  data.month = localTime->tm_mon + 1;
  data.day = localTime->tm_mday;
  data.hour = localTime->tm_hour;
  data.minute = localTime->tm_min;
  data.second = localTime->tm_sec;
}
