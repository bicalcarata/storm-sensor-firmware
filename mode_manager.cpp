#include "mode_manager.h"

#include <string.h>

static const char *MODE_PREF_NAMESPACE = "ld-mode";
static const char *MODE_PREF_KEY = "mode";

void ModeManager::begin(AppState &state) {
  prefs.begin(MODE_PREF_NAMESPACE, false);
  uint8_t saved = prefs.getUChar(MODE_PREF_KEY, (uint8_t)MODE_ROAMING);
  currentMode = saved == (uint8_t)MODE_HOME ? MODE_HOME : MODE_ROAMING;
  setMode(state, currentMode, false);
}

DeviceMode ModeManager::mode() const {
  return currentMode;
}

void ModeManager::setMode(AppState &state, DeviceMode mode, bool persist) {
  currentMode = mode == MODE_HOME ? MODE_HOME : MODE_ROAMING;
  state.mode = currentMode;
  state.connectivity = currentMode == MODE_HOME ? CONN_WIFI : CONN_BLE;
  if (persist) {
    prefs.putUChar(MODE_PREF_KEY, (uint8_t)currentMode);
  }
}

void ModeManager::toggle(AppState &state) {
  setMode(state, currentMode == MODE_HOME ? MODE_ROAMING : MODE_HOME, true);
}
