#include "upload_manager.h"
#include <time.h>

UploadManager::UploadManager(DisplayModule& display,
                         MyWiFiModule& wifi,
                         R2Module& r2,
                         StorageModule& storage)
    : _display(display), _wifi(wifi), _r2(r2), _storage(storage) {
}

bool UploadManager::stopAndUpload(const GNSS_DATA& gnssData) {
  // WiFi設定があれば、指定SSIDを検索してアップロード
  if (!AppConfig::isWifiConfigured()) {
    Serial.println("[UPLOAD] WiFi SSID not configured, skipping upload");
    _display.showMessage("Data saved to SD\n");
    Serial.println("[UPLOAD] Data saved to SD card");
    delay(2000);
    return false;
  }

  const auto* wifiCfg = AppConfig::getWifiConfig();
  Serial.printf("[UPLOAD] WiFi SSID configured: %s\n", wifiCfg->ssid);
  _display.showMessage("Checking WiFi...\n");

  if (!_wifi.isSSIDAvailable(wifiCfg->ssid)) {
    _display.showMessage("WiFi not found\n");
    Serial.println("[UPLOAD] WiFi network not found");
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
    Serial.println("[UPLOAD] R2 credentials not configured, skipping upload");
  }

  // WiFi切断
  _wifi.disconnect();
  _display.showMessage("WiFi disconnected\n");
  delay(1000);

  _display.showMessage("Data saved to SD\n");
  Serial.println("[UPLOAD] Data saved to SD card");
  delay(2000);

  return uploadSuccess;
}

bool UploadManager::_connectWiFi(const WIFI_CONFIG* wifiCfg) {
  _display.showMessage("WiFi found!\n");
  Serial.println("[UPLOAD] WiFi network found, attempting connection...");
  delay(1000);

  // WiFiに接続
  _wifi.begin(wifiCfg->ssid, wifiCfg->password);
  if (!_wifi.connect(30000)) {
    return false;
  }

  Serial.printf("[UPLOAD] WiFi connected successfully\r\n");

  // NTPで時刻同期
  if (_syncTimeWithNTP()) {
    return true;
  }

  // NTP失敗時はGPS時刻を使用
  Serial.printf("[UPLOAD] NTP sync failed, using GPS time\r\n");
  return true;
}

bool UploadManager::_syncTimeWithNTP() {
  // タイムゾーンを0にしてシステム時刻をUTCにする
  configTime(0, 0, NTP_SERVER);
  Serial.printf("[UPLOAD] Syncing time with NTP server: %s\r\n", NTP_SERVER);

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
    Serial.printf("[UPLOAD] NTP sync successful: %s\r\n", timeStr);
    return true;
  }

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
  Serial.printf("[UPLOAD] Using GPS time: %s\r\n", timeStr);
}

bool UploadManager::_uploadToR2() {
  const auto* r2Cfg = AppConfig::getR2Config();
  Serial.printf("[UPLOAD] R2 Account ID: %s\r\n", r2Cfg->accountId);
  Serial.printf("[UPLOAD] R2 Bucket: %s\r\n", r2Cfg->bucketName);
  Serial.printf("[UPLOAD] R2 Region: %s\r\n", r2Cfg->region);
  Serial.printf("[UPLOAD] R2 Access Key length: %d\r\n", strlen(r2Cfg->accessKey));
  _r2.begin(r2Cfg->accountId,
            r2Cfg->bucketName,
            r2Cfg->accessKey,
            r2Cfg->secretKey,
            r2Cfg->region);

  // キューを使ってアップロード
  char filename[128];
  int uploadedCount = 0;
  bool batchFailure = false;

  // キューから順番にアップロード（失敗時は中断）
  while (_storage.getNextFileToUpload(filename, sizeof(filename))) {
    // R2キーを生成：filenameから日付部分を抽出してR2パスを作成
    char remoteKey[256];
    _generateR2Key(filename, remoteKey, sizeof(remoteKey));

    Serial.printf("[UPLOAD] Uploading: %s -> %s\r\n", filename, remoteKey);

    // アップロード試行
    if (_r2.uploadFile(filename, remoteKey)) {
      // 成功したらキューから削除
      _storage.removeFileFromUploadList(filename);
      uploadedCount++;
      _display.showMessage("Uploaded!\n");
      Serial.printf("[UPLOAD] Upload success: %s\r\n", filename);
      delay(500);
    } else {
      // 失敗したら中止
      batchFailure = true;
      break;
    }
  }

  // 結果メッセージ
  if (batchFailure) {
    Serial.println("[UPLOAD] Batch upload failed. Will retry on next startup.");
    return false;
  } else {
    Serial.printf("[UPLOAD] Batch upload complete: %d files\r\n", uploadedCount);
    return true;
  }
}

void UploadManager::_generateR2Key(const char* filename,
                                 char* remoteKey,
                                 size_t remoteKeySize) {
  // filename: "/gnss_csv_data_20250115_143000.csv" -> remoteKey: "gnss-data/20250115/gnss_csv_data_20250115_143000.csv"
  const char* basename = filename + 1;  // 先頭の'/'をスキップ
  char dateStr[9];
  strncpy(dateStr, basename + 14, 8);  // "gnss_csv_data_"の後ろ8文字（YYYYMMDD）
  dateStr[8] = '\0';
  snprintf(remoteKey, remoteKeySize, "gnss-data/%s/%s", dateStr, basename);
}
