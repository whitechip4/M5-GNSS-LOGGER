#ifndef MY_WIFI_H
#define MY_WIFI_H

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"

// WIFI_CONFIG is now defined in config.h to avoid duplication

/**
 * @brief WiFiモジュールクラス
 */
class MyWiFiModule {
public:
  /**
   * @brief コンストラクタ
   */
  MyWiFiModule();

  /**
   * @brief 初期化
   * @param ssid WiFi SSID
   * @param password WiFiパスワード
   */
  void begin(const char* ssid, const char* password);

  /**
   * @brief WiFiに接続
   * @param timeout タイムアウト時間（ミリ秒）
   * @return 接続成功時true
   */
  bool connect(unsigned long timeout = 30000);

  /**
   * @brief WiFi接続を切断
   */
  void disconnect();

  /**
   * @brief WiFi接続状態を確認
   * @return 接続されている場合true
   */
  bool isConnected();

  /**
   * @brief 特定SSIDが利用可能かスキャン
   * @param targetSSID 検索するSSID
   * @return 見つかった場合true
   */
  bool isSSIDAvailable(const char* targetSSID);

  /**
   * @brief WiFi接続を更新（ループ内で定期的に呼ぶ）
   */
  void update();

private:
  char _mySsid[64];
  char _myPassword[64];
  bool _myConnected;
  unsigned long _myLastConnectionCheck;
};

#endif  // MY_WIFI_H