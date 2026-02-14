#ifndef UPLOAD_MANAGER_H
#define UPLOAD_MANAGER_H

#include <Arduino.h>
#include "config.h"
#include "display.h"
#include "my_wifi.h"
#include "r2.h"
#include "storage.h"

/**
 * @brief アップロード処理を管理するマネージャークラス
 *
 * レコーディング停止時のWiFi接続、時刻同期、R2アップロード、
 * WiFi切断処理を一括管理する
 */
class UploadManager {
public:
  /**
   * @brief コンストラクタ
   * @param display ディスプレイモジュールへの参照
   * @param wifi WiFiモジュールへの参照
   * @param r2 R2モジュールへの参照
   * @param storage ストレージモジュールへの参照
   */
  UploadManager(DisplayModule& display, MyWiFiModule& wifi, R2Module& r2, StorageModule& storage);

  /**
   * @brief レコーディング停止とアップロード処理を実行
   *
   * 以下の処理を順次実行する：
   * 1. WiFi接続
   * 2. NTP時刻同期（失敗時はGPS時刻でフォールバック）
   * 3. R2バッチアップロード
   * 4. WiFi切断
   *
   * @param gnssData GNSSデータ（GPS時刻フォールバック用）
   * @return アップロード成功時true
   */
  bool stopAndUpload(const GNSS_DATA& gnssData);

private:
  DisplayModule& _display;
  MyWiFiModule& _wifi;
  R2Module& _r2;
  StorageModule& _storage;

  /**
   * @brief WiFi接続を確立
   * @param wifiCfg WiFi設定
   * @return 接続成功時true
   */
  bool _connectWiFi(const WIFI_CONFIG* wifiCfg);

  /**
   * @brief NTPで時刻同期
   * @return 同期成功時true
   */
  bool _syncTimeWithNTP();

  /**
   * @brief GPS時刻をシステム時刻に設定（NTP失敗時のフォールバック）
   * @param gnssData GNSSデータ
   */
  void _setGPSTime(const GNSS_DATA& gnssData);

  /**
   * @brief R2バッチアップロード実行
   * @return アップロード成功時true
   */
  bool _uploadToR2();

  /**
   * @brief ファイル名からR2キーを生成
   * @param filename ローカルファイルパス
   * @param remoteKey 出力バッファ
   * @param remoteKeySize バッファサイズ
   */
  void _generateR2Key(const char* filename, char* remoteKey, size_t remoteKeySize);
};

#endif  // UPLOAD_MANAGER_H
