#include <Arduino.h>
#include <M5Core2.h>
#include <AXP192.h>
#include <time.h>

#include "config.h"
#include "gnss.h"
#include "display.h"
#include "storage.h"
#include "util.h"
#include "my_wifi.h"
#include "r2.h"

// グローバルインスタンス
GnssModule gnssModule(Serial2);
DisplayModule displayModule;
StorageModule storageModule;
MyWiFiModule wifiModule;
R2Module r2Module;
AXP192 axp192;

// グローバル変数
GNSS_DATA gnssData;
float batVoltage = 0;
bool isGpsOk = false;
bool isSdCardOk = false;
bool isRecording = true;
DISPLAY_MODE viewMode = DISPLAY_MODE_DETAIL;

// ファイル名（setupで初期化）
char fileName[64] = "";
char fileRawDataName[128] = "";

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief UTC時刻のtm構造体をtime_tに変換
 * @param tm UTC時刻のtm構造体
 * @return Unixタイムスタンプ
 *
 * mktime()は入力をローカル時刻として扱うため、
 * ESP32でUTCを扱う場合はこの関数を使用する
 */
time_t timegm_utc(struct tm* tm) {
  int month = tm->tm_mon;
  int year = tm->tm_year + 1900;

  // 各月の積算日数（閏年対応は後で計算）
  static const int days_in_month[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };

  // 1970年からの年数を計算
  long days = (year - 1970) * 365L;

  // 閏年の日数を加算
  for (int y = 1970; y < year; y++) {
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
      days += 1;
    }
  }

  // 今年の経過日数を加算
  days += days_in_month[month];

  // 閏年で2月以降の場合は+1日
  if (month > 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
    days += 1;
  }

  // 日を加算
  days += tm->tm_mday - 1;

  // 秒数に変換
  time_t result = days * 86400L;
  result += tm->tm_hour * 3600L;
  result += tm->tm_min * 60L;
  result += tm->tm_sec;

  return result;
}

void setup() {
  M5.begin(true, true, true, true);
  displayModule.begin();
  Serial2.begin(38400, SERIAL_8N1, 13, 14);  // NEO_M9N用

  displayModule.showMessage("Initializing...\n");

  // GNSSモジュール初期化
  if (!gnssModule.begin()) {
    displayModule.showMessage("u-blox GNSS module not detected");
    delay(5000);
    ESP.restart();
  }

  displayModule.clear();
  displayModule.showMessage("Waiting for receive Time Signal...\n");

  // 時刻信号受信待機
  uint8_t initialTimeSecond = 0;
  do {
    gnssModule.update();
    gnssModule.getData(gnssData);

    displayModule.showMessage("Waiting for receive Time Signal...\n");
    displayModule.showMessage("DT: ");
    char timeStr[64];
    sprintf(timeStr,
            "%04d/%02d/%02d_%02d%02d%02d\n",
            gnssData.year,
            gnssData.month,
            gnssData.day,
            gnssData.hour,
            gnssData.minute,
            gnssData.second);
    displayModule.showMessage(timeStr);

    delay(1000);
    displayModule.clear();
  } while (
      !(gnssData.timeValid && gnssData.dateValid && (gnssData.second != 0) && (gnssData.day != 0)));

  // ファイル名生成（グローバル変数を使用）
  StorageModule::generateFileName("gnss_csv_data", fileName, sizeof(fileName), gnssData, false);
  StorageModule::generateFileName(
      "gnss_csv_data", fileRawDataName, sizeof(fileRawDataName), gnssData, true);

  // SDカード初期化
  if (!storageModule.begin()) {
    displayModule.showMessage("Error : SDCardNotFound");
    delay(10000);
    ESP.restart();
  }

  // ヘッダー書き込み（両方のファイルに）
  storageModule.writeHeader(fileName);
  storageModule.writeHeader(fileRawDataName);

  // 初期データ書き込み
  storageModule.writeData(gnssData, fileName);
  storageModule.writeRawData(gnssData, fileRawDataName);

  // GNSSデータ安定待機
  do {
    displayModule.showMessage("Waiting gnss data be stable...\n");
    char satStr[32];
    sprintf(satStr, "Satellites: %d (>= 7)\n", gnssData.siv);
    displayModule.showMessage(satStr);

    gnssModule.update();
    gnssModule.getData(gnssData);
    isGpsOk = gnssModule.isValid(gnssData);

    delay(500);
    displayModule.clear();
  } while ((!isGpsOk) && (gnssData.siv < 7));

  // 設定を読み込む（.envファイルがない場合は空文字列のまま）
  AppConfig::loadConfig();
}

void _stopRecording() {
  isRecording = false;
  displayModule.clear();
  displayModule.showMessage("Recording stopped\n");
  Serial.println("[MAIN] Recording stopped");
  delay(1000);

  // WiFi設定があれば、指定SSIDを検索してアップロード
  if (AppConfig::isWifiConfigured()) {
    const auto* wifiCfg = AppConfig::getWifiConfig();
    Serial.printf("[MAIN] WiFi SSID configured: %s\n", wifiCfg->ssid);
    displayModule.showMessage("Checking WiFi...\n");

    if (wifiModule.isSSIDAvailable(wifiCfg->ssid)) {
      displayModule.showMessage("WiFi found!\n");
      Serial.println("[MAIN] WiFi network found, attempting connection...");
      delay(1000);

      // WiFiに接続
      wifiModule.begin(wifiCfg->ssid, wifiCfg->password);
      if (wifiModule.connect(30000)) {
        Serial.printf("[MAIN] WiFi connected successfully\r\n");

        // NTPで時刻同期
        // タイムゾーンを0にしてシステム時刻をUTCにする
        configTime(0, 0, NTP_SERVER);
        Serial.printf("[MAIN] Syncing time with NTP server: %s\r\n", NTP_SERVER);

        // NTP同期を待機（最大10秒）
        int ntpTries = 0;
        time_t now = time(nullptr);
        while (now < 1000000000 && ntpTries < 20) {  // 2001年以降なら成功
          delay(500);
          now = time(nullptr);
          ntpTries++;
        }

        // NTP同期完了後に追加で5秒待つ（ESP32のNTP同期処理完了を待つ）
        delay(5000);

        if (now >= 1000000000) {
          struct tm timeinfo;
          gmtime_r(&now, &timeinfo);
          char timeStr[64];
          strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
          Serial.printf("[MAIN] NTP sync successful: %s\r\n", timeStr);
        } else {
          Serial.printf("[MAIN] NTP sync failed, using GPS time\r\n");
          // GPS時刻をシステム時刻に設定（フォールバック）
          // gnssDataはローカル時刻（UTC_TIME_OFFSET_HOURSが加算済み）
          // UTCに戻すために、タイムゾーンオフセットを引く
          struct tm gpsTime;
          gpsTime.tm_year = gnssData.year - 1900;
          gpsTime.tm_mon = gnssData.month - 1;
          gpsTime.tm_mday = gnssData.day;
          gpsTime.tm_hour = gnssData.hour - UTC_TIME_OFFSET_HOURS;  // ローカル→UTC
          gpsTime.tm_min = gnssData.minute;
          gpsTime.tm_sec = gnssData.second;
          // システムタイムゾーンはUTC(0)なのでmktimeでOK
          time_t gpsTimeVal = mktime(&gpsTime);
          struct timeval tv = {gpsTimeVal, 0};
          settimeofday(&tv, nullptr);

          // ログ出力用に元のローカル時刻を復元
          gpsTime.tm_hour = gnssData.hour;
          char timeStr[64];
          strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", &gpsTime);
          Serial.printf("[MAIN] Using GPS time: %s\r\n", timeStr);
        }

        // R2設定があればアップロード
        if (AppConfig::isR2Configured()) {
          const auto* r2Cfg = AppConfig::getR2Config();
          Serial.printf("[MAIN] R2 Account ID: %s\r\n", r2Cfg->accountId);
          Serial.printf("[MAIN] R2 Bucket: %s\r\n", r2Cfg->bucketName);
          Serial.printf("[MAIN] R2 Region: %s\r\n", r2Cfg->region);
          Serial.printf("[MAIN] R2 Access Key length: %d\r\n", strlen(r2Cfg->accessKey));
          r2Module.begin(r2Cfg->accountId, r2Cfg->bucketName, r2Cfg->accessKey, r2Cfg->secretKey, r2Cfg->region);

          // CSVファイルをアップロード
          char remoteKey[256];
          R2Module::generateKey("gnss_csv_data", remoteKey, sizeof(remoteKey), gnssData);
          Serial.printf("[MAIN] Uploading regular CSV file: %s\n", fileName);
          Serial.printf("[MAIN] Remote key: %s\n", remoteKey);

          if (r2Module.uploadFile(fileName, remoteKey)) {
            displayModule.showMessage("CSV uploaded!\n");
            Serial.println("[MAIN] Regular CSV upload successful!");
            delay(1000);
          } else {
            Serial.println("[MAIN] Regular CSV upload failed!");
          }

          // Raw CSVファイルもアップロード
          R2Module::generateKey("gnss_csv_data", remoteKey, sizeof(remoteKey), gnssData);
          size_t keyLen = strlen(remoteKey);
          snprintf(remoteKey + keyLen - 4, sizeof(remoteKey) - keyLen + 4, "_raw.csv");
          Serial.printf("[MAIN] Uploading raw CSV file: %s\n", fileRawDataName);
          Serial.printf("[MAIN] Remote key: %s\n", remoteKey);

          if (r2Module.uploadFile(fileRawDataName, remoteKey)) {
            displayModule.showMessage("Raw CSV uploaded!\n");
            Serial.println("[MAIN] Raw CSV upload successful!");
            delay(1000);
          } else {
            Serial.println("[MAIN] Raw CSV upload failed!");
          }
        } else {
          Serial.println("[MAIN] R2 credentials not configured, skipping upload");
        }

        // WiFi切断
        wifiModule.disconnect();
        displayModule.showMessage("WiFi disconnected\n");
        delay(1000);
      }
    } else {
      displayModule.showMessage("WiFi not found\n");
      Serial.println("[MAIN] WiFi network not found");
      delay(2000);
    }
  } else {
    Serial.println("[MAIN] WiFi SSID not configured, skipping upload");
  }

  displayModule.showMessage("Data saved to SD\n");
  Serial.println("[MAIN] Data saved to SD card");
  delay(2000);
}

void loop() {
  static uint8_t preSecond = 0;

  // 100msec job
  if (!(millis() % 100)) {
    M5.update();
    gnssModule.update();
    gnssModule.getData(gnssData);

    batVoltage = axp192.GetBatVoltage();
    isGpsOk = gnssModule.isValid(gnssData);
    isSdCardOk = storageModule.isReady();

    // 1sec job
    if (gnssData.second != preSecond) {
      preSecond = gnssData.second;

      // 生データ書き込み（記録中のみ）
      if (isRecording) {
        storageModule.writeRawData(gnssData, fileRawDataName);
      }

      // 有効なデータのみ書き込み（記録中のみ）
      if (isRecording && isGpsOk) {
        storageModule.writeData(gnssData, fileName);
      }

      // 表示更新
      displayModule.update(gnssData, batVoltage, isGpsOk, isSdCardOk, viewMode);
    }

    // button A pressed -> change view mode
    if (M5.BtnA.wasPressed()) {
      viewMode = (viewMode == DISPLAY_MODE_DETAIL) ? DISPLAY_MODE_SIMPLE : DISPLAY_MODE_DETAIL;
      vibration(200);
    }

    // button B pressed -> stop recording (with confirmation)
    if (M5.BtnB.wasPressed() && isRecording) {
      displayModule.showMessage("Stop recording?\n");
      displayModule.showMessage("BtnB: Confirm\n");
      displayModule.showMessage("BtnC: Cancel\n");
      vibration(200);

      // 確認ダイアログの待機ループ
      unsigned long confirmStart = millis();
      bool confirmed = false;
      bool cancelled = false;

      while (millis() - confirmStart < 10000) {  // 10秒間待機
        M5.update();
        vibrationProcess();

        if (M5.BtnB.wasPressed()) {
          confirmed = true;
          vibration(200);
          break;
        }

        if (M5.BtnC.wasPressed()) {
          cancelled = true;
          vibration(200);
          break;
        }

        delay(50);
      }

      if (confirmed) {
        stopVibration();  // 振動を停止してからアップロード処理へ
        _stopRecording();
      } else {
        displayModule.clear();
        vibration(200);
        stopVibration();  // 振動を即座に停止
      }
    }

    // 1msec job
    if (!(millis() % 1)) {
      vibrationProcess();
    }
  }
}
