#include "wifi_manager.h"

#include <string.h>

static const uint8_t WIFI_FAILURE_LIMIT = 3;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000UL;
static const uint32_t WIFI_RETRY_INTERVAL_MS = 5000UL;
static const char *WIFI_PREF_NAMESPACE = "ld-home-wifi";
static const char *WIFI_PREF_SSID = "ssid";
static const char *WIFI_PREF_PASS = "pass";

bool DeviceWifiManager::hasSavedCredentials() const {
  return savedSsid[0] != '\0';
}

void DeviceWifiManager::copySavedSsid(AppState &state) {
  if (hasSavedCredentials()) {
    strncpy(state.wifiSsid, savedSsid, sizeof(state.wifiSsid));
  } else {
    strncpy(state.wifiSsid, "--", sizeof(state.wifiSsid));
  }
  state.wifiSsid[sizeof(state.wifiSsid) - 1] = '\0';
}

void DeviceWifiManager::connectWithSavedCredentials(AppState &state) {
  if (!hasSavedCredentials()) {
    connecting = false;
    strncpy(state.wifiStatus, "NO CREDS", sizeof(state.wifiStatus));
    copySavedSsid(state);
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSsid, savedPassword);
  connectStartedMs = millis();
  connecting = true;
  state.wifiEnabled = true;
  strncpy(state.wifiStatus, "CONNECTING", sizeof(state.wifiStatus));
  copySavedSsid(state);
}

void DeviceWifiManager::begin(AppState &state) {
  prefs.begin(WIFI_PREF_NAMESPACE, false);
  prefs.getString(WIFI_PREF_SSID, savedSsid, sizeof(savedSsid));
  prefs.getString(WIFI_PREF_PASS, savedPassword, sizeof(savedPassword));
  WiFi.persistent(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_OFF);
  strncpy(state.wifiStatus, "OFF", sizeof(state.wifiStatus));
  strncpy(state.ipAddress, "0.0.0.0", sizeof(state.ipAddress));
  state.wifiEnabled = false;
  state.wifiConnected = false;
  copySavedSsid(state);
}

void DeviceWifiManager::applyMode(AppState &state, DeviceMode mode) {
  homeRequested = mode == MODE_HOME;
  if (!homeRequested) {
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_OFF);
    connecting = false;
    state.wifiEnabled = false;
    state.wifiConnected = false;
    state.wifiProvisioning = false;
    strncpy(state.wifiStatus, "OFF", sizeof(state.wifiStatus));
    strncpy(state.ipAddress, "0.0.0.0", sizeof(state.ipAddress));
    return;
  }

  WiFi.mode(WIFI_STA);
  state.wifiEnabled = true;
  state.wifiProvisioning = false;
  connectWithSavedCredentials(state);
}

void DeviceWifiManager::update(AppState &state) {
  if (!homeRequested) return;

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    connecting = false;
    state.wifiEnabled = true;
    state.wifiConnected = true;
    state.wifiProvisioning = false;
    state.wifiFailureCount = 0;
    strncpy(state.wifiStatus, "CONNECTED", sizeof(state.wifiStatus));
    strncpy(state.wifiSsid, WiFi.SSID().c_str(), sizeof(state.wifiSsid));
    strncpy(state.ipAddress, WiFi.localIP().toString().c_str(), sizeof(state.ipAddress));
    state.wifiSsid[sizeof(state.wifiSsid) - 1] = '\0';
    state.ipAddress[sizeof(state.ipAddress) - 1] = '\0';
    return;
  }

  state.wifiConnected = false;
  strncpy(state.ipAddress, "0.0.0.0", sizeof(state.ipAddress));

  if (!hasSavedCredentials()) {
    strncpy(state.wifiStatus, "NO CREDS", sizeof(state.wifiStatus));
    copySavedSsid(state);
    return;
  }

  uint32_t now = millis();
  if (connecting && now - connectStartedMs > WIFI_CONNECT_TIMEOUT_MS) {
    connecting = false;
    state.wifiFailureCount++;
    strncpy(state.wifiStatus, "FAILED", sizeof(state.wifiStatus));
    lastRetryMs = now;
  }

  if (!connecting && state.wifiFailureCount < WIFI_FAILURE_LIMIT && now - lastRetryMs >= WIFI_RETRY_INTERVAL_MS) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid, savedPassword);
    connecting = true;
    connectStartedMs = now;
    strncpy(state.wifiStatus, "RETRYING", sizeof(state.wifiStatus));
  }
}

void DeviceWifiManager::clearSavedCredentials(AppState &state) {
  WiFi.disconnect(true, true);
  prefs.remove(WIFI_PREF_SSID);
  prefs.remove(WIFI_PREF_PASS);
  savedSsid[0] = '\0';
  savedPassword[0] = '\0';
  state.wifiFailureCount = 0;
  state.wifiConnected = false;
  copySavedSsid(state);
  strncpy(state.ipAddress, "0.0.0.0", sizeof(state.ipAddress));
  strncpy(state.wifiStatus, "RESET", sizeof(state.wifiStatus));
}

void DeviceWifiManager::noteProvisionedCredentials(AppState &state, const char *ssid, const char *password) {
  strncpy(savedSsid, ssid, sizeof(savedSsid));
  strncpy(savedPassword, password, sizeof(savedPassword));
  savedSsid[sizeof(savedSsid) - 1] = '\0';
  savedPassword[sizeof(savedPassword) - 1] = '\0';
  prefs.putString(WIFI_PREF_SSID, savedSsid);
  prefs.putString(WIFI_PREF_PASS, savedPassword);
  WiFi.mode(WIFI_STA);
  state.wifiFailureCount = 0;
  homeRequested = true;
  connectWithSavedCredentials(state);
}

bool DeviceWifiManager::shouldProvision(const AppState &state) const {
  return homeRequested && !state.wifiConnected &&
         (!hasSavedCredentials() || state.wifiFailureCount >= WIFI_FAILURE_LIMIT);
}
