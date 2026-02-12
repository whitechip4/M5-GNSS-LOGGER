#include "upload_manager.h"
#include <time.h>
#include "util.h"

UploadManager::UploadManager(DisplayModule& display,
                             MyWiFiModule& wifi,
                             R2Module& r2,
                             StorageModule& storage)
    : _display(display)
    , _wifi(wifi)
    , _r2(r2)
    , _storage(storage) {
}

bool UploadManager::stopAndUpload(const GNSS_DATA& gnssData) {
  // WiFi設定があれば、指定SSIDを検索してアップロード
  if (!AppConfig::isWifiConfigured()) {
    debug_print("UPLOAD", " WiFi SSID not configured, skipping upload");
    _display.logMessage("Data saved to SD");
    debug_print("UPLOAD", " Data saved to SD card");
    delay(2000);
    return false;
  }

  const auto* wifiCfg = AppConfig::getWifiConfig();
  debug_print("UPLOAD", " WiFi SSID configured: %s", wifiCfg->ssid);
  _display.logMessage("Checking WiFi...");

  if (!_wifi.isSSIDAvailable(wifiCfg->ssid)) {
    _display.logMessage("WiFi not found");
    debug_print("UPLOAD", " WiFi network not found");
    delay(2000);
    return false;
  }

  if (!_connectWiFi(wifiCfg)) {
    return false;
  }

  // R2設定があればアップロード
  bool uploadSuccess = false;
  if (AppConfig::isR2Configured()) {
    uploadSuccess = _uploadToR2();
  } else {
    debug_print("UPLOAD", " R2 credentials not configured, skipping upload");
  }

  // WiFi切断
  _wifi.disconnect();
  _display.logMessage("WiFi disconnected");
  delay(1000);

  _display.logMessage("Data saved to SD");
  debug_print("UPLOAD", " Data saved to SD card");
  delay(2000);

  return uploadSuccess;
}

bool UploadManager::_connectWiFi(const WIFI_CONFIG* wifiCfg) {
  _display.logMessage("WiFi found!");
  debug_print("UPLOAD", " WiFi network found, attempting connection...");
  delay(1000);

  // WiFiに接続
  _wifi.begin(wifiCfg->ssid, wifiCfg->password);
  if (!_wifi.connect(30000)) {
    return false;
  }

  debug_print("UPLOAD", " WiFi connected successfully");

  // NTPで時刻同期
  if (_syncTimeWithNTP()) {
    return true;
  }

  // NTP失敗時はGPS時刻を使用
  debug_print("UPLOAD", " NTP sync failed, using GPS time");
  return true;
}

bool UploadManager::_syncTimeWithNTP() {
  // タイムゾーンを0にしてシステム時刻をUTCにする
  configTime(0, 0, NTP_SERVER);

  _display.logMessage("Syncing NTP time...");
  debug_print("UPLOAD", " Syncing time with NTP server: %s", NTP_SERVER);

  // NTP同期を待機（最大10秒）
  int ntpTries = 0;
  time_t now = time(nullptr);
  while (now < 1000000000 && ntpTries < 20) {  // 2001年以降なら成功
    delay(500);
    now = time(nullptr);
    ntpTries++;

    // Progress indicator on LCD
    if (ntpTries % 4 == 0) {  // Every 2 seconds
      _display.logProgress(".");
    }
  }

  // NTP同期完了後に追加で5秒待つ（ESP32のNTP同期処理完了を待つ）
  _display.logMessage("Waiting...");
  delay(5000);

  if (now >= 1000000000) {
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char timeStr[64];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);

    _display.logMessage("Time synced!");
    debug_print("UPLOAD", " NTP sync successful: %s", timeStr);
    delay(1000);
    return true;
  }

  // NTP失敗時のメッセージ
  _display.logMessage("NTP failed, using GPS time");
  debug_print("UPLOAD", " NTP sync failed, using GPS time");
  delay(1000);
  return false;
}

void UploadManager::_setGPSTime(const GNSS_DATA& gnssData) {
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
  debug_print("UPLOAD", " Using GPS time: %s", timeStr);
}

bool UploadManager::_uploadToR2() {
  const auto* r2Cfg = AppConfig::getR2Config();
  debug_print("UPLOAD", " R2 Account ID: %s", r2Cfg->accountId);
  debug_print("UPLOAD", " R2 Bucket: %s", r2Cfg->bucketName);
  debug_print("UPLOAD", " R2 Region: %s", r2Cfg->region);
  debug_print("UPLOAD", " R2 Access Key length: %d", strlen(r2Cfg->accessKey));
  _r2.begin(r2Cfg->accountId, r2Cfg->bucketName, r2Cfg->accessKey, r2Cfg->secretKey, r2Cfg->region);

  _display.logMessage("Starting upload...");

  // キューを使ってアップロード
  char filename[128];
  int uploadedCount = 0;
  bool batchFailure = false;

  // キューから順番にアップロード（失敗時は中断）
  while (_storage.getNextFileToUpload(filename, sizeof(filename))) {
    // R2キーを生成：filenameから日付部分を抽出してR2パスを作成
    char remoteKey[256];
    _generateR2Key(filename, remoteKey, sizeof(remoteKey));

    debug_print("UPLOAD", " Uploading: %s -> %s", filename, remoteKey);

    // ファイル名を表示（basenameのみ）
    const char* basename = filename;
    if (basename[0] == '/') {
      basename++;  // 先頭の'/'をスキップ
    }

    // アップロード開始メッセージ
    char uploadMsg[144];
    snprintf(uploadMsg, sizeof(uploadMsg), "Uploading: %s...", basename);
    _display.logMessage(uploadMsg);

    // アップロード試行（ストリーミング版を使用）
    if (_r2.uploadFileStream(filename, remoteKey)) {
      // 成功したらキューから削除
      _storage.removeFileFromUploadList(filename);
      uploadedCount++;

      // 成功メッセージ（ファイル名付き）
      char successMsg[144];
      snprintf(successMsg, sizeof(successMsg), "Uploaded: %s", basename);
      _display.logMessage(successMsg);

      debug_print("UPLOAD", " Upload success: %s", filename);
      delay(500);
    } else {
      // 失敗したら中止
      char failMsg[144];
      snprintf(failMsg, sizeof(failMsg), "Upload failed: %s", basename);
      _display.logMessage(failMsg);

      batchFailure = true;
      break;
    }
  }

  // 結果メッセージ
  if (batchFailure) {
    _display.logMessage("Upload failed. Will retry later.");
    debug_print("UPLOAD", " Batch upload failed. Will retry on next startup.");
    return false;
  } else {
    char completeMsg[64];
    snprintf(completeMsg, sizeof(completeMsg), "Upload complete: %d files", uploadedCount);
    _display.logMessage(completeMsg);

    debug_print("UPLOAD", " Batch upload complete: %d files", uploadedCount);
    return true;
  }
}

void UploadManager::_generateR2Key(const char* filename, char* remoteKey, size_t remoteKeySize) {
  // filename: "/gnss_csv_data_20250115_143000.csv" -> remoteKey:
  // "gnss-data/20250115/gnss_csv_data_20250115_143000.csv"
  const char* basename = filename + 1;  // 先頭の'/'をスキップ
  char dateStr[9];
  strncpy(dateStr, basename + 14, 8);  // "gnss_csv_data_"の後ろ8文字（YYYYMMDD）
  dateStr[8] = '\0';
  snprintf(remoteKey, remoteKeySize, "gnss-data/%s/%s", dateStr, basename);
}
