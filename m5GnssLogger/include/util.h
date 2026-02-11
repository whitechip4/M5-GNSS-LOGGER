#ifndef UTIL_H
#define UTIL_H

#include <Arduino.h>
#include <M5Core2.h>
#include <AXP192.h>

/**
 * @brief 振動を開始
 * @param ms 振動時間（ミリ秒）
 */
void vibration(uint32_t ms);

/**
 * @brief 振動プロセス（定期的に呼び出す）
 */
void vibrationProcess();

/**
 * @brief 振動を即座に停止
 */
void stopVibration();

// ============================================================================
// Debug Logging
// ============================================================================
#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1
#endif

#if DEBUG_ENABLED
/**
 * @brief Unified debug print function
 * @param tag Module tag (e.g., "MAIN", "R2", "WiFi")
 * @param format Printf-style format string
 * @param ... Variable arguments
 */
void debug_print(const char* tag, const char* format, ...);
#else
#define debug_print(...)
#endif

// ============================================================================
// Time Utilities
// ============================================================================
/**
 * @brief Convert UTC tm struct to time_t (UTC-aware mktime)
 * @param tm UTC time struct
 * @return Unix timestamp
 *
 * mktime() treats input as local time, use this for UTC on ESP32
 */
time_t timegm_utc(struct tm* tm);

#endif  // UTIL_H
