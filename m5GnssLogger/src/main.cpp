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

void _runTimezoneSettingMode() {
  int8_t offset = AppConfig::getTimezoneOffset();

  displayModule.clear();
  displayModule.logMessage("== Timezone Setting ==");
  displayModule.logMessage("BtnA: -1h / BtnC: +1h");
  displayModule.logMessage("BtnB: Save & Exit");

  char offsetStr[BUFFER_TIME_STR];
  sprintf(offsetStr, "Timezone: UTC%+d", offset);
  displayModule.logMessage(offsetStr);

  // 無操作タイムアウト管理（誤操作で入場した場合は保存せず通常起動に戻る）
  uint32_t lastActivityMs = millis();
  bool timedOut = false;

  // 起動時の長押しが-1h操作として誤検出されないよう、一度離されるまで待つ
  // （何かが画面に触れ続けている場合もタイムアウトで抜ける）
  while (M5.BtnA.isPressed() && !timedOut) {
    M5.update();
    timedOut = (millis() - lastActivityMs > DELAY_TIMEZONE_SETTING_TIMEOUT_MS);
    delay(DELAY_BUTTON_POLL_MS);
  }

  while (!timedOut) {
    M5.update();
    vibrationProcess();

    if (millis() - lastActivityMs > DELAY_TIMEZONE_SETTING_TIMEOUT_MS) {
      timedOut = true;
      break;
    }

    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
      lastActivityMs = millis();
    }

    bool changed = false;
    if (M5.BtnA.wasPressed() && offset > TIMEZONE_OFFSET_MIN) {
      offset--;
      changed = true;
    }
    if (M5.BtnC.wasPressed() && offset < TIMEZONE_OFFSET_MAX) {
      offset++;
      changed = true;
    }
    if (changed) {
      vibration(100);
      sprintf(offsetStr, "Timezone: UTC%+d", offset);
      displayModule.logMessage(offsetStr);
    }

    if (M5.BtnB.wasPressed()) {
      AppConfig::setTimezoneOffset(offset);
      AppConfig::saveTimezoneToNvs();
      vibration(200);
      displayModule.logMessage("Saved!");
      delay(DELAY_GENERAL_TIMING_MS);
      break;
    }

    delay(DELAY_BUTTON_POLL_MS);
  }

  if (timedOut) {
    displayModule.logMessage("Timeout: not saved");
    delay(DELAY_GENERAL_TIMING_MS);
  }

  stopVibration();
  displayModule.clear();
}

void setup() {
  // M5Core2 initialization: (SDEnable, SerialEnable, LCDEnable, I2CEnable)
  // Note: LCD is disabled here to avoid conflict with LovyanGFX
  M5.begin(true, true, false, true);
  displayModule.begin();
  Serial2.begin(38400, SERIAL_8N1, 13, 14);  // NEO_M9N用

  displayModule.clear();

  // NVSに保存されたタイムゾーンを読み込む（未保存時はコンパイル時デフォルト値）
  AppConfig::loadTimezoneFromNvs();

  // 起動直後のボタンA押下でのみタイムゾーン設定モードに入る
  // （タッチボタンの誤操作防止のため、通常起動では設定画面を出さない）
  // 注意: タッチパネルは電源投入時に静電容量をキャリブレーションするため、
  // 電源ON前から押しっぱなしの指はベースラインとして無効化され検出できない。
  // このためプロンプト表示後に押してもらう方式とし、一定時間ポーリングして判定する
  displayModule.logMessage("Hold BtnA for TZ setting...");
  const uint32_t touchCheckStart = millis();
  while (millis() - touchCheckStart < BOOT_BUTTON_CHECK_MS) {
    M5.update();
    if (M5.BtnA.isPressed()) {
      _runTimezoneSettingMode();
      break;
    }
    delay(DELAY_BUTTON_POLL_MS);
  }

  displayModule.logMessage("Initializing...");

  // タイムゾーン表示
  char timezoneStr[BUFFER_TIME_STR];
  sprintf(timezoneStr, "Timezone: UTC%+d", AppConfig::getTimezoneOffset());
  displayModule.logMessage(timezoneStr);

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

  // 初期データ書き込み（GNSSデータが有効な場合のみ）
  if (gnssModule.isValid(gnssData)) {
    storageModule.writeData(gnssData, fileName);
    storageModule.writeRawData(gnssData, fileRawDataName);
    debug_print("MAIN", "Initial data recorded");
  }

  // アップロードキューにファイルを追加
  storageModule.addFileToUploadList(fileName);
  // Raw CSVファイルはR2にアップロードしない（SDカードには保存継続）

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
