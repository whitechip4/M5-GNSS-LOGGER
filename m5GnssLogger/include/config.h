#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief GNSSデータ構造体
 */
typedef struct {
  uint8_t siv;  // sattellites number in view
  int32_t latRaw;
  int32_t lngRaw;
  double lat;  // doubleはM5Stack Core2では32bit...
  double lng;

  double alt;  // [m]
  float vel;   // [km/h]

  bool dateValid;
  uint16_t year;
  uint16_t month;
  uint16_t day;

  bool timeValid;
  uint16_t hour;
  uint16_t minute;
  uint16_t second;
  uint16_t millisecond;

  uint8_t fixType;  // 0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix
  float hdop;
  float pdop;

  bool isFixOk;

} GNSS_DATA;

/**
 * @brief GNSS thresholds
 */
#define GNSS_HDOP_THRESHOLD 6.0f
#define GNSS_MIN_SATELLITES 5
#define GNSS_RECOVERY_BUFFER_MS 5000
#define GNSS_POSITION_THRESHOLD 0.001f

/**
 * @brief バッテリー設定
 */
#define BATTERY_VOLTAGE_THRESHOLD 3.6f

/**
 * @brief UTC時差設定（時間単位）
 * 日本標準時: +9
 * 台湾時間: +8
 */
#define UTC_TIME_OFFSET_HOURS 9

/**
 * @brief NTPサーバー設定
 */
#define NTP_SERVER "amazon.pool.ntp.org"  // AWS公開NTP
#define NTP_TIMEOUT 10000                 // 10秒

// タイムゾーン（日本時間: UTC+9 = 9時間 = 32400秒）
#define TIMEZONE_OFFSET 32400

/**
 * @brief 表示モード
 */
typedef enum {
  DISPLAY_MODE_DETAIL = 0,
  DISPLAY_MODE_SIMPLE = 1
} DISPLAY_MODE;

// ============================================================================
// WiFi Configuration
// ============================================================================

/**
 * @brief WiFi設定構造体
 */
typedef struct {
  char ssid[64];
  char password[64];
} WIFI_CONFIG;

// ============================================================================
// R2 Configuration
// ============================================================================

/**
 * @brief R2設定構造体
 */
typedef struct {
  char accountId[64];
  char bucketName[64];
  char accessKey[128];
  char secretKey[128];
  char region[32];
} R2_CONFIG;

// ============================================================================
// Application Configuration Namespace
// ============================================================================

/**
 * @brief アプリケーション全体の設定を管理する名前空間
 */
namespace AppConfig {

/**
 * @brief WiFi設定を取得
 * @return WiFi設定構造体へのポインタ
 */
const WIFI_CONFIG* getWifiConfig();

/**
 * @brief R2設定を取得
 * @return R2設定構造体へのポインタ
 */
const R2_CONFIG* getR2Config();

/**
 * @brief 設定を.env.hのプリプロセッサマクロから読み込む
 *
 * この関数はsetup()内で呼び出し、.env.hで定義された
 * WIFI_SSID, R2_ACCOUNT_IDなどのマクロから実行時設定にコピーします
 *
 * .env.hファイルが存在しない場合、各設定は空文字列のままです
 */
void loadConfig();

/**
 * @brief WiFi SSIDが設定されているか確認
 * @return 設定されている場合true
 */
bool isWifiConfigured();

/**
 * @brief R2設定がされているか確認
 * @return 設定されている場合true
 */
bool isR2Configured();

// 内部実装用（config.cppで定義）
extern WIFI_CONFIG _wifiConfig;
extern R2_CONFIG _r2Config;

}  // namespace AppConfig

#endif  // CONFIG_H
