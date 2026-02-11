#ifndef MOCK_M5CORE2_H
#define MOCK_M5CORE2_H

// Mock M5Core2 library for native testing

#include "mock_arduino.h"

// Mock M5 class
class M5 {
public:
  static void begin(bool LCDEnable = true,
                    bool SDEnable = true,
                    bool SerialEnable = true,
                    bool I2CEnable = false) {
  }
  static void update() {
  }
};

#endif  // MOCK_M5CORE2_H
