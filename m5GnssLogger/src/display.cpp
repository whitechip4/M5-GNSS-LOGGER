#include "display.h"

DisplayModule::DisplayModule()
    : _lcd()
    , _sprite(&_lcd) {
  // _sprite holds a reference to _lcd
}

DisplayModule::~DisplayModule() {
  _sprite.deleteSprite();
}

void DisplayModule::begin() {
  // Initialize LovyanGFX display
  // Called after M5.begin()
  _lcd.init();
  _lcd.setRotation(1);
  _lcd.setBrightness(128);

  // Set sprite color depth (8-bit for memory efficiency)
  _sprite.setColorDepth(8);

  // Create sprite in internal RAM (320x240x1 bytes = ~76KB)
  // PSRAM is not used, placed in internal RAM
  if (!_sprite.createSprite(320, 240)) {
    _lcd.println("ERROR: Failed to create sprite in internal RAM");
  } else {
    _lcd.println("Sprite created successfully in internal RAM");
  }

  // Default sprite settings
  _sprite.setTextFont(2);
  _sprite.setTextSize(1);
  _sprite.setTextColor(COLOR_TEXT);
  _sprite.setCursor(0, 0);
}

void DisplayModule::clear() {
  _sprite.fillScreen(COLOR_BG);
  _sprite.setCursor(0, 0);
}

void DisplayModule::showMessage(const char* message) {
  clear();
  _sprite.setTextColor(COLOR_TEXT);
  _sprite.println(message);
  _sprite.pushSprite(0, 0);  // Immediately transfer to screen
}

void DisplayModule::logMessage(const char* message) {
  // Set text properties
  _sprite.setTextColor(COLOR_TEXT);
  _sprite.setTextSize(1);

  // Check if screen is full, clear and reset if needed
  if (_logCursorY >= LOG_SCREEN_HEIGHT) {
    _sprite.fillScreen(COLOR_BG);
    _logCursorY = 0;
    _sprite.setCursor(0, 0);
  } else {
    _sprite.setCursor(0, _logCursorY);
  }

  // Print message at current cursor position
  _sprite.println(message);

  // Save cursor position after this message (for logProgress)
  _lastLogEndX = _sprite.getCursorX();
  _lastLogEndY = _sprite.getCursorY();

  // Update cursor position for next message
  _logCursorY = _sprite.getCursorY();

  // Immediately transfer to screen
  _sprite.pushSprite(0, 0);
}

void DisplayModule::setDisplayMode(DISPLAY_MODE mode) {
  // Switch display mode - could add initialization logic here if needed
  // For now, just store the mode (could be used for future features)
  // Mode change is handled by update() method
}

void DisplayModule::resetLogCursor() {
  _logCursorY = 0;
  _lastLogEndX = 0;
  _lastLogEndY = 0;
  _sprite.fillScreen(COLOR_BG);
  _sprite.setCursor(0, 0);
  _sprite.pushSprite(0, 0);
}

void DisplayModule::logProgress(const char* text) {
  // Append text to the last log message line
  // Use the saved cursor position from the last logMessage
  _sprite.setTextColor(COLOR_TEXT);
  _sprite.setTextSize(1);
  _sprite.setCursor(_lastLogEndX, _lastLogEndY);

  // Print text without newline (use print instead of println)
  _sprite.print(text);

  // Update the saved position for next progress update
  _lastLogEndX = _sprite.getCursorX();
  _lastLogEndY = _sprite.getCursorY();

  // Also update main cursor if we moved to new line
  if (_lastLogEndY > _logCursorY) {
    _logCursorY = _lastLogEndY;
  }

  // Immediately transfer to screen
  _sprite.pushSprite(0, 0);
}

void DisplayModule::update(const GNSS_DATA& data,
                           float batteryVoltage,
                           bool isGpsOk,
                           bool isSdCardOk,
                           bool isRecording,
                           DISPLAY_MODE mode) {
  // Clear sprite buffer (off-screen)
  clear();

  // Draw status line to sprite
  _sprite.setTextColor(COLOR_TEXT);
  _sprite.print("Status : ");

  if (isRecording) {
    _sprite.setTextColor(COLOR_STATUS_OK);
    _sprite.println("Recording");
  } else {
    _sprite.setTextColor(COLOR_STATUS_ERROR);
    _sprite.println("Stop");
  }

  // Reset text color to white for subsequent content
  _sprite.setTextColor(COLOR_TEXT);

  // Draw mode-specific content to sprite
  if (mode == DISPLAY_MODE_DETAIL) {
    _showDetailMode(data, batteryVoltage, isGpsOk, isSdCardOk);
  } else {
    _showSimpleMode(data);
  }

  // Atomic operation: transfer entire sprite to screen
  _sprite.pushSprite(0, 0);
}

void DisplayModule::_showDetailMode(const GNSS_DATA& data,
                                    float batteryVoltage,
                                    bool isGpsOk,
                                    bool isSdCardOk) {
  _sprite.setTextColor(COLOR_TEXT);
  _sprite.setTextSize(1);

  _sprite.printf("Satellites: %d\n", data.siv);
  _sprite.printf("hdop: %.2f\n", data.hdop);
  _sprite.printf("pdop: %.2f\n", data.pdop);
  _sprite.printf("isFixOK: %d\n", data.isFixOk);
  _sprite.printf("fixtype: %d\n", data.fixType);
  _sprite.printf("Lat: %.6f\n", data.lat);
  _sprite.printf("Lon: %.6f\n", data.lng);
  _sprite.printf("Alt: %.6f\n", data.alt);
  _sprite.printf("Vel: %.1f\n", data.vel);
  _sprite.printf("DT: %04d/%02d/%02d_%02d%02d%02d\n",
                 data.year,
                 data.month,
                 data.day,
                 data.hour,
                 data.minute,
                 data.second);
  _sprite.printf("BAT: %.2f\n", batteryVoltage);
}

void DisplayModule::_showSimpleMode(const GNSS_DATA& data) {
  // Speed display (large)
  _sprite.setTextSize(5);
  _sprite.printf("%5.0f", data.vel);
  _sprite.setTextSize(2);
  _sprite.printf(" km/h");
  _sprite.setTextSize(5);
  _sprite.printf("\n");

  // Separator line
  _sprite.setTextSize(1);
  _sprite.println("-----------------");

  // Altitude
  _sprite.setTextSize(3);
  _sprite.printf("%7.0f", data.alt);
  _sprite.setTextSize(2);
  _sprite.printf("   m high\n");
  _sprite.setTextSize(3);
  _sprite.printf("\n");

  // Time
  _sprite.setTextSize(2);
  _sprite.printf("Time: %02d:%02d:%02d\n", data.hour, data.minute, data.second);

  // Reset to default
  _sprite.setTextSize(1);
}
