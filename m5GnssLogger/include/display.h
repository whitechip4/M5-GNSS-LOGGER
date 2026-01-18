#ifndef DISPLAY_H
#define DISPLAY_H

#include <M5Core2.h>
#include "config.h"

/**
 * @brief ディスプレイモジュールクラス
 */
class DisplayModule {
public:
  /**
   * @brief コンストラクタ
   */
  DisplayModule();

  /**
   * @brief 初期化
   */
  void begin();

  /**
   * @brief ディスプレイを更新
   * @param data 表示するGNSSデータ
   * @param batteryVoltage バッテリー電圧
   * @param isGpsOk GPS状態
   * @param isSdCardOk SDカード状態
   * @param mode 表示モード
   */
  void update(const GNSS_DATA &data, float batteryVoltage, 
              bool isGpsOk, bool isSdCardOk, DISPLAY_MODE mode);

  /**
   * @brief クリア
   */
  void clear();

  /**
   * @brief メッセージ表示
   * @param message 表示するメッセージ
   */
  void showMessage(const char *message);

private:
  /**
   * @brief 詳細モードで表示
   */
  void _showDetailMode(const GNSS_DATA &data, float batteryVoltage, 
                       bool isGpsOk, bool isSdCardOk);

  /**
   * @brief シンプルモードで表示
   */
  void _showSimpleMode(const GNSS_DATA &data);
};

#endif // DISPLAY_H
