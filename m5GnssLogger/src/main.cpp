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

// Include configuration from .env.h file
#include ".env.h"

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

// 設定（.envから読み込む）
char wifiSsid[64] = "";
char wifiPassword[64] = "";
char r2AccountId[64] = "";
char r2BucketName[64] = "";
char r2AccessKey[128] = "";
char r2SecretKey[128] = "";
char r2Region[32] = "auto";

void _loadConfig();

// ファイル名（setupで初期化）
char fileName[64] = "";
char fileRawDataName[128] = "";

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
  _loadConfig();
}

void _loadConfig() {
  // Load settings from auto-generated config_env.h
  // This file is generated from .env file during build
  
#ifdef WIFI_SSID
  strncpy(wifiSsid, WIFI_SSID, sizeof(wifiSsid) - 1);
  wifiSsid[sizeof(wifiSsid) - 1] = '\0';
#endif

#ifdef WIFI_PASSWORD
  strncpy(wifiPassword, WIFI_PASSWORD, sizeof(wifiPassword) - 1);
  wifiPassword[sizeof(wifiPassword) - 1] = '\0';
#endif

#ifdef R2_ACCOUNT_ID
  strncpy(r2AccountId, R2_ACCOUNT_ID, sizeof(r2AccountId) - 1);
  r2AccountId[sizeof(r2AccountId) - 1] = '\0';
#endif

#ifdef R2_BUCKET_NAME
  strncpy(r2BucketName, R2_BUCKET_NAME, sizeof(r2BucketName) - 1);
  r2BucketName[sizeof(r2BucketName) - 1] = '\0';
#endif

#ifdef R2_ACCESS_KEY
  strncpy(r2AccessKey, R2_ACCESS_KEY, sizeof(r2AccessKey) - 1);
  r2AccessKey[sizeof(r2AccessKey) - 1] = '\0';
#endif

#ifdef R2_SECRET_KEY
  strncpy(r2SecretKey, R2_SECRET_KEY, sizeof(r2SecretKey) - 1);
  r2SecretKey[sizeof(r2SecretKey) - 1] = '\0';
#endif

#ifdef R2_REGION
  strncpy(r2Region, R2_REGION, sizeof(r2Region) - 1);
  r2Region[sizeof(r2Region) - 1] = '\0';
#endif
}

void _stopRecording() {
  isRecording = false;
  displayModule.clear();
  displayModule.showMessage("Recording stopped\n");
  delay(1000);

  // WiFi設定があれば、指定SSIDを検索してアップロード
  if (strlen(wifiSsid) > 0) {
    displayModule.showMessage("Checking WiFi...\n");
    
    if (wifiModule.isSSIDAvailable(wifiSsid)) {
      displayModule.showMessage("WiFi found!\n");
      delay(1000);
      
      // WiFiに接続
      wifiModule.begin(wifiSsid, wifiPassword);
      if (wifiModule.connect(30000)) {
        // R2設定があればアップロード
        if (strlen(r2AccountId) > 0 && strlen(r2AccessKey) > 0) {
          r2Module.begin(r2AccountId, r2BucketName, r2AccessKey, r2SecretKey, r2Region);
          
          // CSVファイルをアップロード
          char remoteKey[256];
          R2Module::generateKey("gnss_csv_data", remoteKey, sizeof(remoteKey), gnssData);
          
          if (r2Module.uploadFile(fileName, remoteKey)) {
            displayModule.showMessage("CSV uploaded!\n");
            delay(1000);
          }
          
          // Raw CSVファイルもアップロード
          R2Module::generateKey("gnss_csv_data", remoteKey, sizeof(remoteKey), gnssData);
          size_t keyLen = strlen(remoteKey);
          snprintf(remoteKey + keyLen - 4, sizeof(remoteKey) - keyLen + 4, "_raw.csv");
          
          if (r2Module.uploadFile(fileRawDataName, remoteKey)) {
            displayModule.showMessage("Raw CSV uploaded!\n");
            delay(1000);
          }
        }
        
        // WiFi切断
        wifiModule.disconnect();
        displayModule.showMessage("WiFi disconnected\n");
        delay(1000);
      }
    } else {
      displayModule.showMessage("WiFi not found\n");
      delay(2000);
    }
  }

  displayModule.showMessage("Data saved to SD\n");
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
        _stopRecording();
      } else {
        displayModule.clear();
        vibration(200);
      }
    }

    // 1msec job
    if (!(millis() % 1)) {
      vibrationProcess();
    }
  }
}
