#include "util.h"

// 振動制御変数（ファイルスコープ）
static uint32_t vibEndTimeMillis = 0;
static bool vibFlag = false;

/**
 * @brief 振動を開始
 * @param ms 振動時間（ミリ秒）
 */
void vibration(uint32_t ms) {
  vibEndTimeMillis = millis() + ms;
  vibFlag = true;
  M5.Axp.SetLDOEnable(3, true);
}

/**
 * @brief 振動プロセス（定期的に呼び出す）
 */
void vibrationProcess() {
  if (!vibFlag) {
    return;
  }

  if (millis() >= vibEndTimeMillis) {
    vibFlag = false;
    M5.Axp.SetLDOEnable(3, false);  // 振動モーターをOFF
  }
}
