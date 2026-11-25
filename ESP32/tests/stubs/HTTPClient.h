#pragma once
// Host stand-in for arduino-esp32's HTTPClient. Nothing goes on the network:
// every request is appended to HttpLog so a test can inspect what the sketch
// would have sent, and HttpLog.nextStatus picks the reply code.
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <string>
#include <vector>

struct FakeHttpRequest {
  std::string method, url, body;
  std::vector<std::string> headers;  // "Name: value"
};

struct FakeHttpLog {
  std::vector<FakeHttpRequest> requests;
  int nextStatus = 200;  // e.g. 401 for a bad secret, -1 for a connect failure
  unsigned begins = 0, ends = 0;
};
inline FakeHttpLog HttpLog;

class HTTPClient {
 public:
  bool begin(WiFiClientSecure &, const String &url) {
    ++HttpLog.begins;
    pending_ = FakeHttpRequest{};
    pending_.url = url;
    headers_.clear();
    return true;
  }
  void addHeader(const String &name, const String &value) {
    headers_.push_back(name + ": " + value);
  }
  void setReuse(bool reuse) { reuse_ = reuse; }
  void setTimeout(uint16_t ms) { timeoutMs_ = ms; }
  int sendRequest(const char *method, const String &payload) {
    pending_.method = method;
    pending_.body = payload;
    pending_.headers = headers_;
    HttpLog.requests.push_back(pending_);
    return HttpLog.nextStatus;
  }
  int PUT(const String &payload) { return sendRequest("PUT", payload); }
  int PATCH(const String &payload) { return sendRequest("PATCH", payload); }
  int POST(const String &payload) { return sendRequest("POST", payload); }
  int GET() { return sendRequest("GET", ""); }
  String getString() { return ""; }
  void end() { ++HttpLog.ends; }

 private:
  FakeHttpRequest pending_;
  std::vector<std::string> headers_;
  bool reuse_ = false;
  uint16_t timeoutMs_ = 5000;
};
