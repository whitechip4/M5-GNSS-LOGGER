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

/**
 * @brief 振動を即座に停止
 */
void stopVibration() {
  vibFlag = false;
  M5.Axp.SetLDOEnable(3, false);  // 振動モーターをOFF
}

// ============================================================================
// Debug Logging
// ============================================================================
#if DEBUG_ENABLED
#include <stdarg.h>

void debug_print(const char* tag, const char* format, ...) {
  Serial.print("[");
  Serial.print(tag);
  Serial.print("] ");

  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  Serial.print(buffer);
  Serial.print("\r\n");
}
#endif

// ============================================================================
// Time Utilities
// ============================================================================
#include <time.h>

time_t timegm_utc(struct tm* tm) {
  int month = tm->tm_mon;
  int year = tm->tm_year + 1900;

  // 各月の積算日数（閏年対応は後で計算）
  static const int days_in_month[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };

  // 1970年からの年数を計算
  long days = (year - 1970) * 365L;

  // 閏年の日数を加算
  for (int y = 1970; y < year; y++) {
    if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
      days += 1;
    }
  }

  // 今年の経過日数を加算
  days += days_in_month[month];

  // 閏年で2月以降の場合は+1日
  if (month > 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
    days += 1;
  }

  // 日を加算
  days += tm->tm_mday - 1;

  // 秒数に変換
  time_t result = days * 86400L;
  result += tm->tm_hour * 3600L;
  result += tm->tm_min * 60L;
  result += tm->tm_sec;

  return result;
}
