#include <Arduino.h>
#include <M5Core2.h>
#include <AXP192.h>
#include <time.h>

#include "config.h"
#include "gnss.h"
#include "display.h"
#include "storage.h"

// グローバルインスタンス
GnssModule gnssModule(Serial2);
DisplayModule displayModule;
StorageModule storageModule;
AXP192 axp192;

// グローバル変数
GNSS_DATA gnssData;
float batVoltage = 0;
bool isGpsOk = false;
bool isSdCardOk = false;
DISPLAY_MODE viewMode = DISPLAY_MODE_DETAIL;

// ファイル名（setupで初期化）
char fileName[64] = "";
char fileRawDataName[128] = "";

// 振動制御変数
uint32_t vibEndTimeMillis = 0;
bool vibFlag = false;

// 関数プロトタイプ
static void vibration(uint32_t ms);
static void vibrationProcess();

/**
 * @brief 振動を開始
 * @param ms 振動時間（ミリ秒）
 */
static void vibration(uint32_t ms) {
  vibEndTimeMillis = millis() + ms;
  vibFlag = true;
  M5.Axp.SetLDOEnable(3, true);
}

/**
 * @brief 振動プロセス（定期的に呼び出す）
 */
static void vibrationProcess() {
  if (!vibFlag) {
    return;
  }

  if (millis() >= vibEndTimeMillis) {
    vibFlag = false;
    M5.Axp.SetLDOEnable(3, false);  // 振動モーターをOFF
  }
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

      // 生データ書き込み（常に）
      storageModule.writeRawData(gnssData, fileRawDataName);

      // 有効なデータのみ書き込み
      if (isGpsOk) {
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

    // 1msec job
    if (!(millis() % 1)) {
      vibrationProcess();
    }
  }
}
