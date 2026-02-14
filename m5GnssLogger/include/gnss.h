#ifndef GNSS_H
#define GNSS_H

#include <Arduino.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#include "config.h"

/**
 * @brief GNSSモジュールクラス
 */
class GnssModule {
public:
  /**
   * @brief コンストラクタ
   * @param serial シリアルポート
   */
  GnssModule(HardwareSerial& serial);

  /**
   * @brief 初期化
   * @return 成功時true
   */
  bool begin();

  /**
   * @brief GNSSデータを取得
   * @param data データを格納するGNSS_DATA構造体への参照
   */
  void getData(GNSS_DATA& data);

  /**
   * @brief GNSSデータが有効かチェック
   * @param data チェックするGNSS_DATA構造体
   * @return 有効な場合true
   */
  bool isValid(const GNSS_DATA& data);

  /**
   * @brief GNSSモジュールを更新（定期的に呼び出す）
   */
  void update();

private:
  SFE_UBLOX_GNSS _gnss;
  HardwareSerial& _serial;
  uint32_t _recoveryBufferTimeAnchor;

  /**
   * @brief UTC Unix時間からJST時間を設定
   * @param utcTime UTC Unix時間
   * @param data 設定するGNSS_DATA構造体への参照
   */
  void _setJstTimeFromUTCUnixTime(time_t utcTime, GNSS_DATA& data);
};

#endif  // GNSS_H
