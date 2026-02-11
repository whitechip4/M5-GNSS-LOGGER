#ifndef MOCK_HTTPCLIENT_H
#define MOCK_HTTPCLIENT_H

// Include Arduino mocks first for String class
#include "mock_arduino.h"

// Mock HTTPClient for native testing
// This provides minimal stubs to allow compilation

class HTTPClient {
public:
  HTTPClient() {
  }
  ~HTTPClient() {
  }

  bool begin(const char* url) {
    (void)url;
    return true;
  }
  void end() {
  }

  int addHeader(const char* name, const char* value) {
    (void)name;
    (void)value;
    return 1;
  }

  int PUT(const uint8_t* data, size_t len) {
    (void)data;
    (void)len;
    return 200;
  }

  String getString() {
    return String("");
  }
};

#endif  // MOCK_HTTPCLIENT_H
