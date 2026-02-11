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
char fileName[BUFFER_FILENAME_MAX] = "";
char fileRawDataName[BUFFER_FILENAME_RAW_MAX] = "";

void setup() {
  M5.begin(true, true, true, true);
  displayModule.begin();
  Serial2.begin(38400, SERIAL_8N1, 13, 14);  // NEO_M9N用

  displayModule.showMessage("Initializing...\n");

  // GNSSモジュール初期化
  if (!gnssModule.begin()) {
    displayModule.showMessage("u-blox GNSS module not detected");
    delay(DELAY_GNSS_INIT_MS);
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
    char timeStr[BUFFER_TIME_STR];
    sprintf(timeStr,
            "%04d/%02d/%02d_%02d%02d%02d\n",
            gnssData.year,
            gnssData.month,
            gnssData.day,
            gnssData.hour,
            gnssData.minute,
            gnssData.second);
    displayModule.showMessage(timeStr);

    delay(DELAY_GENERAL_TIMING_MS);
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
    delay(DELAY_SD_ERROR_DISPLAY_MS);
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
    char satStr[BUFFER_SAT_STR];
    sprintf(satStr, "Satellites: %d (>= 7)\n", gnssData.siv);
    displayModule.showMessage(satStr);

    gnssModule.update();
    gnssModule.getData(gnssData);
    isGpsOk = gnssModule.isValid(gnssData);

    delay(DELAY_GNSS_STABILITY_CHECK_MS);
    displayModule.clear();
  } while ((!isGpsOk) && (gnssData.siv < 7));

  // 設定を読み込む（.envファイルがない場合は空文字列のまま）
  AppConfig::loadConfig();
}

void _stopRecording() {
  isRecording = false;
  displayModule.clear();
  displayModule.showMessage("Recording stopped\n");
  debug_print("MAIN", "Recording stopped");
  delay(DELAY_GENERAL_TIMING_MS);

  // アップロードマネージャーで処理
  uploadManager.stopAndUpload(gnssData);
}

void _showStopConfirmationDialog() {
  displayModule.showMessage("Stop recording?\n");
  displayModule.showMessage("BtnB: Confirm\n");
  displayModule.showMessage("BtnC: Cancel\n");
  vibration(200);

  unsigned long confirmStart = millis();
  bool confirmed = false;
  bool cancelled = false;

  while (millis() - confirmStart < DELAY_CONFIRM_TIMEOUT_MS) {
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

    delay(DELAY_BUTTON_POLL_MS);
  }

  if (confirmed) {
    stopVibration();
    _stopRecording();
  } else {
    displayModule.clear();
    vibration(200);
    stopVibration();
  }
}

void loop() {
  static uint8_t preSecond = 0;

  // 100msec job
  if (!(millis() % LOOP_INTERVAL_MS)) {
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
      _showStopConfirmationDialog();
    }

    // 1msec job
    if (!(millis() % LOOP_INTERVAL_VIBRATION_MS)) {
      vibrationProcess();
    }
  }
}
