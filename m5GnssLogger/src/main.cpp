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
  // M5Core2 initialization: (SDEnable, SerialEnable, LCDEnable, I2CEnable)
  // Note: LCD is disabled here to avoid conflict with LovyanGFX
  M5.begin(true, true, false, true);
  displayModule.begin();
  Serial2.begin(38400, SERIAL_8N1, 13, 14);  // NEO_M9N用

  displayModule.clear();
  displayModule.logMessage("Initializing...");

  // GNSSモジュール初期化
  if (!gnssModule.begin()) {
    displayModule.logMessage("Error: u-blox GNSS module not detected");
    delay(DELAY_GNSS_INIT_MS);
    ESP.restart();
  }

  // 時刻信号受信待機
  uint8_t initialTimeSecond = 0;
  displayModule.logMessage("Waiting for receive Time Signal...");
  do {
    gnssModule.update();
    gnssModule.getData(gnssData);

    char timeStr[BUFFER_TIME_STR];
    sprintf(timeStr,
            "DT: %04d/%02d/%02d_%02d%02d%02d",
            gnssData.year,
            gnssData.month,
            gnssData.day,
            gnssData.hour,
            gnssData.minute,
            gnssData.second);
    displayModule.logMessage(timeStr);

    delay(DELAY_GENERAL_TIMING_MS);
  } while (
      !(gnssData.timeValid && gnssData.dateValid && (gnssData.second != 0) && (gnssData.day != 0)));

  // ファイル名生成（グローバル変数を使用）
  StorageModule::generateFileName("gnss_csv_data", fileName, sizeof(fileName), gnssData, false);
  StorageModule::generateFileName(
      "gnss_csv_data", fileRawDataName, sizeof(fileRawDataName), gnssData, true);

  // SDカード初期化
  if (!storageModule.begin()) {
    displayModule.logMessage("Error: SDCardNotFound");
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
  displayModule.logMessage("Waiting gnss data be stable...");
  do {
    char satStr[BUFFER_SAT_STR];
    sprintf(satStr, "Satellites: %d (>= 7)", gnssData.siv);
    displayModule.logMessage(satStr);

    gnssModule.update();
    gnssModule.getData(gnssData);
    isGpsOk = gnssModule.isValid(gnssData);

    delay(DELAY_GNSS_STABILITY_CHECK_MS);
  } while ((!isGpsOk) && (gnssData.siv < 7));

  // 設定を読み込む（.envファイルがない場合は空文字列のまま）
  AppConfig::loadConfig();
}

void _stopRecording() {
  isRecording = false;
  displayModule.clear();  // Clear confirmation dialog before starting log mode
  displayModule.logMessage("Recording stopped");
  debug_print("MAIN", "Recording stopped");
  delay(DELAY_GENERAL_TIMING_MS);

  // アップロードマネージャーで処理
  uploadManager.stopAndUpload(gnssData);
}

void _showStopConfirmationDialog() {
  // Clear screen and reset log cursor before showing dialog
  displayModule.resetLogCursor();

  displayModule.logMessage("Stop recording?");
  displayModule.logMessage("BtnB: Confirm");
  displayModule.logMessage("BtnC: Cancel");
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
    displayModule.resetLogCursor();
    vibration(200);
    stopVibration();
  }
}

void loop() {
  static uint8_t preSecond = 0;
  static uint32_t lastDisplayUpdate = 0;

  // Button processing (every loop) - for better response
  M5.update();
  vibrationProcess();

  // Button A: change view mode
  if (M5.BtnA.wasPressed()) {
    viewMode = (viewMode == DISPLAY_MODE_DETAIL) ? DISPLAY_MODE_SIMPLE : DISPLAY_MODE_DETAIL;
    vibration(200);
  }

  // Button B: stop recording (with confirmation)
  if (M5.BtnB.wasPressed() && isRecording) {
    _showStopConfirmationDialog();
  }

  // 100msec job
  if (!(millis() % LOOP_INTERVAL_MS)) {
    gnssModule.update();
    gnssModule.getData(gnssData);

    batVoltage = axp192.GetBatVoltage();
    isGpsOk = gnssModule.isValid(gnssData);
    isSdCardOk = storageModule.isReady();

    // Display update (configurable frequency)
    if (millis() - lastDisplayUpdate >= DELAY_DISPLAY_UPDATE_MS) {
      displayModule.update(gnssData, batVoltage, isGpsOk, isSdCardOk, isRecording, viewMode);
      lastDisplayUpdate = millis();
    }

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
    }
  }
}
