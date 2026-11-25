#pragma once
// Host stand-in for arduino-esp32's WiFiClientSecure: no TLS, only records
// which certificate mode the sketch chose.
struct WiFiClientSecure {
  bool insecure = false;
  const char *caCert = nullptr;
  void setInsecure() { insecure = true; }
  void setCACert(const char *pem) { caCert = pem; }
};
