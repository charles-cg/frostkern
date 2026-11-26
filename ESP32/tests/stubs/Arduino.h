#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <deque>
#define ARDUINO_XIAO_ESP32C5 1
#define ARDUINO_USB_CDC_ON_BOOT 1
#define D7 12
#define D6 11
#define SERIAL_8N1 0
// Minimal Arduino String: enough for c_str(), +, += and passing to HTTPClient.
struct String : std::string {
  using std::string::string;
  String() = default;
  String(const std::string &s) : std::string(s) {}
};
inline uint32_t fakeMs = 0;
inline uint32_t millis() { return fakeMs; }
inline void delay(unsigned ms) { fakeMs += ms; }
struct TestSerial {
  std::string out;
  std::deque<uint8_t> incoming;
  void begin(unsigned, unsigned = 0, int = -1, int = -1) {}
  void setRxBufferSize(unsigned) {}
  explicit operator bool() const { return true; }
  int available() { return (int)incoming.size(); }
  int read() { if (incoming.empty()) return -1; int b = incoming.front(); incoming.pop_front(); return b; }
  void print(const char *s) { out += s; }
  void println(const char *s = "") { out += s; out += "\r\n"; }
  void println(unsigned long n) { out += std::to_string(n); out += "\r\n"; }
  void printf(const char *format, ...) {
    char b[1024]; va_list args; va_start(args, format);
    vsnprintf(b, sizeof(b), format, args); va_end(args); out += b;
  }
};
inline TestSerial Serial, Serial1;
