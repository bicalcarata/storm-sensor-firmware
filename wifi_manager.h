#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include "app_state.h"

class DeviceWifiManager {
public:
  void begin(AppState &state);
  void applyMode(AppState &state, DeviceMode mode);
  void update(AppState &state);
  void clearSavedCredentials(AppState &state);
  void noteProvisionedCredentials(AppState &state, const char *ssid, const char *password);
  bool shouldProvision(const AppState &state) const;

private:
  bool hasSavedCredentials() const;
  void copySavedSsid(AppState &state);
  void connectWithSavedCredentials(AppState &state);

  Preferences prefs;
  uint32_t connectStartedMs = 0;
  uint32_t lastRetryMs = 0;
  bool connecting = false;
  bool homeRequested = false;
  char savedSsid[33] = {0};
  char savedPassword[65] = {0};
};
