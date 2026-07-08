#include "config.h"
#include ".env.h"
#include <Preferences.h>
#include <string.h>

// ============================================================================
// Timezone Configuration
// ============================================================================
// タイムゾーンオフセット（時間単位）
// NVSに保存値があれば起動時にloadTimezoneFromNvs()で上書きされる
// この値はNVS未保存時のフォールバック（デフォルト: JST=UTC+9）
int8_t timezoneOffsetHours = 9;

// NVSの名前空間とキー
static const char* kNvsNamespace = "gnss_logger";
static const char* kNvsKeyTimezone = "tz_offset";

// ============================================================================
// Application Configuration Namespace
// ============================================================================

/**
 * @brief アプリケーション全体の設定を管理する名前空間
 */
namespace AppConfig {

// WiFi設定（デフォルトは空文字列）
WIFI_CONFIG _wifiConfig = {"", ""};

// R2設定（デフォルトは空文字列）
R2_CONFIG _r2Config = {"", "", "", "", "auto"};

// ============================================================================
// Configuration Accessors
// ============================================================================

const WIFI_CONFIG* getWifiConfig() {
  return &_wifiConfig;
}

const R2_CONFIG* getR2Config() {
  return &_r2Config;
}

bool isWifiConfigured() {
  return strlen(_wifiConfig.ssid) > 0;
}

bool isR2Configured() {
  return strlen(_r2Config.accountId) > 0 && strlen(_r2Config.accessKey) > 0;
}

int8_t getTimezoneOffset() {
  return timezoneOffsetHours;
}

void setTimezoneOffset(int8_t offsetHours) {
  if (offsetHours < TIMEZONE_OFFSET_MIN) {
    offsetHours = TIMEZONE_OFFSET_MIN;
  }
  if (offsetHours > TIMEZONE_OFFSET_MAX) {
    offsetHours = TIMEZONE_OFFSET_MAX;
  }
  timezoneOffsetHours = offsetHours;
}

void loadTimezoneFromNvs() {
  Preferences prefs;
  // 名前空間が未作成（一度も保存していない）場合はbeginが失敗し、デフォルト値を維持する
  if (prefs.begin(kNvsNamespace, true)) {
    setTimezoneOffset(prefs.getChar(kNvsKeyTimezone, timezoneOffsetHours));
    prefs.end();
  }
}

void saveTimezoneToNvs() {
  Preferences prefs;
  if (prefs.begin(kNvsNamespace, false)) {
    prefs.putChar(kNvsKeyTimezone, timezoneOffsetHours);
    prefs.end();
  }
}

// ============================================================================
// Configuration Loading from Preprocessor Macros
// ============================================================================

void loadConfig() {
  // Load settings from auto-generated .env.h
  // This file is generated from .env file during build
  // If .env.h doesn't exist, all settings remain as empty strings

#ifdef WIFI_SSID
  strncpy(_wifiConfig.ssid, WIFI_SSID, sizeof(_wifiConfig.ssid) - 1);
  _wifiConfig.ssid[sizeof(_wifiConfig.ssid) - 1] = '\0';
#endif

#ifdef WIFI_PASSWORD
  strncpy(_wifiConfig.password, WIFI_PASSWORD, sizeof(_wifiConfig.password) - 1);
  _wifiConfig.password[sizeof(_wifiConfig.password) - 1] = '\0';
#endif

#ifdef R2_ACCOUNT_ID
  strncpy(_r2Config.accountId, R2_ACCOUNT_ID, sizeof(_r2Config.accountId) - 1);
  _r2Config.accountId[sizeof(_r2Config.accountId) - 1] = '\0';
#endif

#ifdef R2_BUCKET_NAME
  strncpy(_r2Config.bucketName, R2_BUCKET_NAME, sizeof(_r2Config.bucketName) - 1);
  _r2Config.bucketName[sizeof(_r2Config.bucketName) - 1] = '\0';
#endif

#ifdef R2_ACCESS_KEY
  strncpy(_r2Config.accessKey, R2_ACCESS_KEY, sizeof(_r2Config.accessKey) - 1);
  _r2Config.accessKey[sizeof(_r2Config.accessKey) - 1] = '\0';
#endif

#ifdef R2_SECRET_KEY
  strncpy(_r2Config.secretKey, R2_SECRET_KEY, sizeof(_r2Config.secretKey) - 1);
  _r2Config.secretKey[sizeof(_r2Config.secretKey) - 1] = '\0';
#endif

#ifdef R2_REGION
  strncpy(_r2Config.region, R2_REGION, sizeof(_r2Config.region) - 1);
  _r2Config.region[sizeof(_r2Config.region) - 1] = '\0';
#endif
}

}  // namespace AppConfig
