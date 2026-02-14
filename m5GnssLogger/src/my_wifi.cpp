#include "my_wifi.h"
#include "display.h"
#include "util.h"

// Global display module instance (declared in main.cpp)
extern DisplayModule displayModule;

MyWiFiModule::MyWiFiModule()
    : _myConnected(false)
    , _myLastConnectionCheck(0) {
  _mySsid[0] = '\0';
  _myPassword[0] = '\0';
}

void MyWiFiModule::begin(const char* ssid, const char* password) {
  strncpy(_mySsid, ssid, sizeof(_mySsid) - 1);
  _mySsid[sizeof(_mySsid) - 1] = '\0';

  strncpy(_myPassword, password, sizeof(_myPassword) - 1);
  _myPassword[sizeof(_myPassword) - 1] = '\0';

  WiFi.mode(WIFI_STA);
}

bool MyWiFiModule::connect(unsigned long timeout) {
  if (strlen(_mySsid) == 0) {
    return false;
  }

  displayModule.logMessage("Connecting to WiFi...");
  debug_print("WiFi", "Connecting to: %s", _mySsid);

  WiFi.begin(_mySsid, _myPassword);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) {
    delay(500);
    displayModule.logProgress(".");
    debug_print("WiFi", "Status: %d", WiFi.status());
  }

  if (WiFi.status() == WL_CONNECTED) {
    _myConnected = true;
    displayModule.logMessage("WiFi connected!");
    debug_print("WiFi", "Connected! IP: %s", WiFi.localIP().toString().c_str());
    delay(1000);
    return true;
  } else {
    _myConnected = false;
    displayModule.logMessage("WiFi connection failed");
    debug_print("WiFi", "Connection failed after %lu ms", millis() - start);
    delay(2000);
    return false;
  }
}

void MyWiFiModule::disconnect() {
  if (_myConnected) {
    WiFi.disconnect();
    _myConnected = false;
    displayModule.logMessage("WiFi disconnected");
  }
}

bool MyWiFiModule::isConnected() {
  // Update connection status
  if (millis() - _myLastConnectionCheck > 5000) {
    _myLastConnectionCheck = millis();
    _myConnected = (WiFi.status() == WL_CONNECTED);
  }
  return _myConnected;
}

bool MyWiFiModule::isSSIDAvailable(const char* targetSSID) {
  if (targetSSID == nullptr || strlen(targetSSID) == 0) {
    return false;
  }

  displayModule.logMessage("Scanning WiFi...");

  int numNetworks = WiFi.scanNetworks();
  debug_print("WiFi", "Scanning... Found %d networks", numNetworks);

  for (int i = 0; i < numNetworks; i++) {
    debug_print("WiFi", "Network %d: %s (RSSI: %d)", i, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    if (strcmp(WiFi.SSID(i).c_str(), targetSSID) == 0) {
      WiFi.scanDelete();
      debug_print("WiFi", "Target SSID '%s' found!", targetSSID);
      return true;
    }
  }

  WiFi.scanDelete();
  debug_print("WiFi", "Target SSID '%s' not found", targetSSID);
  return false;
}

void MyWiFiModule::update() {
  // Periodically check connection status
  isConnected();
}