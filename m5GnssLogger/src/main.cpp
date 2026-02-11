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
#include "upload_manager.h"

// グローバルインスタンス
GnssModule gnssModule(Serial2);
DisplayModule displayModule;
StorageModule storageModule;
MyWiFiModule wifiModule;
R2Module r2Module;
AXP192 axp192;
UploadManager uploadManager(displayModule, wifiModule, r2Module, storageModule);

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

  // アップロードキューにファイルを追加
  storageModule.addFileToUploadList(fileName);
  storageModule.addFileToUploadList(fileRawDataName);

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

  // アップロードマネージャーで処理
  uploadManager.stopAndUpload(gnssData);
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
