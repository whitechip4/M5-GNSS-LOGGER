#ifndef MOCK_DISPLAY_H
#define MOCK_DISPLAY_H

// Mock Display module for native testing

#include "mock_arduino.h"

class DisplayModule {
public:
  DisplayModule() {
  }
  void showMessage(const char* msg) {
    (void)msg;
  }
  void update() {
  }
};

// Global instance that r2.cpp references
extern DisplayModule displayModule;

#endif  // MOCK_DISPLAY_H
