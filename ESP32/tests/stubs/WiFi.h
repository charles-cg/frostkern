#pragma once
#include <string>
#define WIFI_STA 1
#define WL_CONNECTED 3
struct FakeIp {std::string toString() const {return "192.0.2.10";}};
struct FakeWiFi {
  bool connected=false;
  unsigned begins=0,reconnects=0;
  void mode(int) {}
  void setAutoReconnect(bool) {}
  void begin(const char *,const char *) {++begins;}
  int status() {return connected?WL_CONNECTED:0;}
  FakeIp localIP() {return {};}
  void reconnect() {++reconnects;}
};
inline FakeWiFi WiFi;
