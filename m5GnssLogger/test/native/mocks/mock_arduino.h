#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

// Mock Arduino functions and types for native testing
// This allows testing on host machine without Arduino framework

#include <string>
#include <cstring>
#include <cstddef>

// Arduino String mock using std::string
class String {
private:
  std::string _str;

  // Private constructor from std::string for internal use
  explicit String(const std::string& s)
      : _str(s) {
  }

public:
  String()
      : _str() {
  }
  String(const char* cstr)
      : _str(cstr ? cstr : "") {
  }
  String(const String& str)
      : _str(str._str) {
  }
  explicit String(int val)
      : _str(std::to_string(val)) {
  }
  explicit String(unsigned int val)
      : _str(std::to_string(val)) {
  }
  explicit String(long val)
      : _str(std::to_string(val)) {
  }
  explicit String(unsigned long val)
      : _str(std::to_string(val)) {
  }
  explicit String(float val)
      : _str(std::to_string(val)) {
  }
  explicit String(double val)
      : _str(std::to_string(val)) {
  }

  // Assignment
  String& operator=(const String& str) {
    _str = str._str;
    return *this;
  }
  String& operator=(const char* cstr) {
    _str = cstr ? cstr : "";
    return *this;
  }

  // Concatenation
  String operator+(const String& str) const {
    return String(_str + str._str);
  }
  String operator+(const char* cstr) const {
    return String(_str + (cstr ? cstr : ""));
  }
  String& operator+=(const String& str) {
    _str += str._str;
    return *this;
  }
  String& operator+=(const char* cstr) {
    _str += cstr ? cstr : "";
    return *this;
  }

  // Comparison
  bool operator==(const String& str) const {
    return _str == str._str;
  }
  bool operator==(const char* cstr) const {
    return _str == (cstr ? cstr : "");
  }
  bool operator!=(const String& str) const {
    return _str != str._str;
  }
  int compareTo(const String& str) const {
    return _str.compare(str._str);
  }
  bool equals(const String& str) const {
    return _str == str._str;
  }

  // Access
  char charAt(unsigned int index) const {
    return _str.at(index);
  }
  char operator[](unsigned int index) const {
    return _str[index];
  }
  const char* c_str() const {
    return _str.c_str();
  }

  // Properties
  unsigned int length() const {
    return _str.length();
  }
  void clear() {
    _str.clear();
  }
  bool isEmpty() const {
    return _str.empty();
  }

  // Search
  int indexOf(char ch) const {
    size_t pos = _str.find(ch);
    return pos == std::string::npos ? -1 : pos;
  }
  int indexOf(const String& str) const {
    size_t pos = _str.find(str._str);
    return pos == std::string::npos ? -1 : pos;
  }
  int indexOf(const char* cstr) const {
    size_t pos = _str.find(cstr ? cstr : "");
    return pos == std::string::npos ? -1 : pos;
  }
  int lastIndexOf(char ch) const {
    size_t pos = _str.rfind(ch);
    return pos == std::string::npos ? -1 : pos;
  }

  // Substring
  String substring(unsigned int beginIndex) const {
    return String(_str.substr(beginIndex).c_str());
  }
  String substring(unsigned int beginIndex, unsigned int endIndex) const {
    return String(_str.substr(beginIndex, endIndex - beginIndex).c_str());
  }

  // Conversion
  int toInt() const {
    return std::stoi(_str);
  }
  float toFloat() const {
    return std::stof(_str);
  }
  double toDouble() const {
    return std::stod(_str);
  }

  // Formatting
  static String format(int val) {
    return String(std::to_string(val).c_str());
  }
};

// Mock Serial class
class HardwareSerial {
public:
  void begin(unsigned long baud) {
    (void)baud;
  }
  void end() {
  }
  int available() {
    return 0;
  }
  int read() {
    return -1;
  }
  int peek() {
    return -1;
  }
  void flush() {
  }

  size_t write(uint8_t c) {
    (void)c;
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t size) {
    (void)buffer;
    return size;
  }

  // printf support
  size_t printf(const char* format, ...) {
    (void)format;
    return 0;
  }
};

extern HardwareSerial Serial;

// Mock basic types
typedef unsigned int uint;
typedef unsigned long ulong;

// Mock time functions (using system struct tm)
inline time_t time(time_t* timer) {
  if (timer)
    *timer = 0;
  return 0;
}

inline struct tm* gmtime_r(const time_t* timer, struct tm* result) {
  (void)timer;
  if (result) {
    result->tm_sec = 0;
    result->tm_min = 0;
    result->tm_hour = 0;
    result->tm_mday = 1;
    result->tm_mon = 0;
    result->tm_year = 125;  // 2025
    result->tm_wday = 3;
    result->tm_yday = 0;
    result->tm_isdst = 0;
  }
  return result;
}

inline size_t strftime(char* s, size_t max, const char* fmt, const struct tm* tm) {
  (void)max;
  (void)fmt;
  (void)tm;
  if (s)
    s[0] = '\0';
  return 0;
}

// Free function for const char* + String concatenation
inline String operator+(const char* cstr, const String& str) {
  String result(cstr);
  result += str;
  return result;
}

// Mock millis()
inline unsigned long millis() {
  return 0;
}

// Mock delay
inline void delay(unsigned long ms) {
  (void)ms;
}

#endif  // MOCK_ARDUINO_H
