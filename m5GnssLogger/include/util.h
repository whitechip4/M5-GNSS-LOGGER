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

#endif // UTIL_H
