#include "my_wifi.h"
#include "display.h"

// Global display module instance (declared in main.cpp)
extern DisplayModule displayModule;

MyWiFiModule::MyWiFiModule()
    : _myConnected(false), _myLastConnectionCheck(0) {
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

  displayModule.showMessage("Connecting to WiFi...\n");

  WiFi.begin(_mySsid, _myPassword);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) {
    delay(500);
    displayModule.showMessage(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    _myConnected = true;
    displayModule.showMessage("WiFi connected!\n");
    delay(1000);
    return true;
  } else {
    _myConnected = false;
    displayModule.showMessage("WiFi connection failed\n");
    delay(2000);
    return false;
  }
}

void MyWiFiModule::disconnect() {
  if (_myConnected) {
    WiFi.disconnect();
    _myConnected = false;
    displayModule.showMessage("WiFi disconnected\n");
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

  displayModule.showMessage("Scanning WiFi...\n");

  int numNetworks = WiFi.scanNetworks();

  for (int i = 0; i < numNetworks; i++) {
    if (strcmp(WiFi.SSID(i).c_str(), targetSSID) == 0) {
      WiFi.scanDelete();
      return true;
    }
  }

  WiFi.scanDelete();
  return false;
}

const char* MyWiFiModule::getCurrentSSID() {
  if (_myConnected) {
    return WiFi.SSID().c_str();
  }
  return nullptr;
}

void MyWiFiModule::update() {
  // Periodically check connection status
  isConnected();
}