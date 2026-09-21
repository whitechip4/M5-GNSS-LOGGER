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

  // ジャンプ検出用: 最後に有効と判定した位置
  bool _hasLastValid;
  double _lastValidLat;
  double _lastValidLng;
  double _lastValidAlt;
  uint32_t _lastValidMs;

  // 持続的な高度変化検出用: 30秒以上前の有効点（slow anchor）と、次にslow anchorへ昇格する候補
  bool _hasSlowAnchor;
  double _slowLat;
  double _slowLng;
  double _slowAlt;
  uint32_t _slowMs;
  bool _hasSlowPending;
  double _slowPendingLat;
  double _slowPendingLng;
  double _slowPendingAlt;
  uint32_t _slowPendingMs;

  // ジャンプ検出用: ジャンプ先候補位置（一貫性が続けば新位置として受け入れる）
  bool _hasJumpCandidate;
  double _candidateLat;
  double _candidateLng;
  double _candidateAlt;
  uint32_t _candidateMs;
  uint32_t _candidateStartMs;

  /**
   * @brief UTC Unix時間からローカル時間を設定
   * @param utcTime UTC Unix時間
   * @param data 設定するGNSS_DATA構造体への参照
   */
  void _setLocalTimeFromUTCUnixTime(time_t utcTime, GNSS_DATA& data);

  /**
   * @brief 2点間の水平距離を計算（ハーバサイン公式）
   * @return 距離 [m]
   */
  static double _distanceM(double lat1, double lng1, double lat2, double lng2);

  /**
   * @brief 高度差が水平移動距離に対して物理的にあり得る勾配内かチェック
   * @param horizontalM 水平移動距離 [m]
   * @param deltaAltM 高度差 [m]
   * @return 勾配が許容範囲内ならtrue
   */
  static bool _isVerticalPlausible(double horizontalM, double deltaAltM);

  /**
   * @brief 有効点を採用した際にslow anchorの状態を更新
   */
  void _updateSlowAnchor(const GNSS_DATA& data, uint32_t nowMs);

  /**
   * @brief 基準位置から見て物理的に到達可能な範囲内かチェック
   * @param fromLat 基準緯度
   * @param fromLng 基準経度
   * @param fromAlt 基準高度 [m]
   * @param fromMs 基準時刻 [ms]
   * @param data チェック対象のGNSSデータ
   * @param nowMs 現在時刻 [ms]
   * @return 到達可能範囲内ならtrue
   */
  static bool _isWithinEnvelope(double fromLat,
                                double fromLng,
                                double fromAlt,
                                uint32_t fromMs,
                                const GNSS_DATA& data,
                                uint32_t nowMs);
};

#endif  // GNSS_H
