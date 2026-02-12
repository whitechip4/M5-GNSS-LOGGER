#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

#ifdef TESTING
#include "native/mocks/mock_m5core2.h"

class DisplayModule {
public:
  DisplayModule() = default;
  ~DisplayModule() = default;
  void begin();
  void update(const GNSS_DATA& data,
              float batteryVoltage,
              bool isGpsOk,
              bool isSdCardOk,
              DISPLAY_MODE mode);
  void clear();
  void showMessage(const char* message);
  void logMessage(const char* message);
  void logProgress(const char* text);  // Append to current line for progress dots
  void setDisplayMode(DISPLAY_MODE mode);
  void resetLogCursor();  // Reset log cursor to top of screen

private:
  void _showDetailMode(const GNSS_DATA& data, float batteryVoltage, bool isGpsOk, bool isSdCardOk);
  void _showSimpleMode(const GNSS_DATA& data);
};

#else
#include <M5Core2.h>

// LovyanGFX for M5Stack Core2 - must be included after M5Core2.h
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>

// Use autodetected LGFX class
using LGFX = lgfx::LGFX;

/**
 * @brief Display module class (double buffering support)
 */
class DisplayModule {
public:
  DisplayModule();
  ~DisplayModule();

  void begin();
  void update(const GNSS_DATA& data,
              float batteryVoltage,
              bool isGpsOk,
              bool isSdCardOk,
              DISPLAY_MODE mode);
  void clear();
  void showMessage(const char* message);
  void logMessage(const char* message);
  void logProgress(const char* text);  // Append to current line for progress dots
  void setDisplayMode(DISPLAY_MODE mode);
  void resetLogCursor();  // Reset log cursor to top of screen

private:
  // LovyanGFX: LCD driver and sprite buffer
  LGFX _lcd;
  LGFX_Sprite _sprite;

  // Color constants
  static constexpr uint16_t COLOR_BG = TFT_BLACK;
  static constexpr uint16_t COLOR_TEXT = TFT_WHITE;
  static constexpr uint16_t COLOR_STATUS_OK = TFT_GREEN;
  static constexpr uint16_t COLOR_STATUS_ERROR = TFT_RED;

  // Log mode state
  int32_t _logCursorY = 0;
  int32_t _logLineHeight = 16;
  static constexpr int32_t LOG_SCREEN_HEIGHT = 240;
  int32_t _lastLogEndX = 0;  // X position after last logMessage
  int32_t _lastLogEndY = 0;  // Y position after last logMessage

  void _showDetailMode(const GNSS_DATA& data, float batteryVoltage, bool isGpsOk, bool isSdCardOk);
  void _showSimpleMode(const GNSS_DATA& data);
};

#endif  // TESTING

#endif  // DISPLAY_H
